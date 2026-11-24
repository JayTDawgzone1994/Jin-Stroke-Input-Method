#include "runtime.hpp"
#include "keyboard_policy.hpp"
#include "input_mode_button.hpp"
#include <ctffunc.h>
#include "candidate_element.hpp"
#include "write_session.hpp"
#include "composition_attributes.hpp"
#include <stroke/engine/continuous.hpp>
#include <stroke/engine/association.hpp>
#include "../storage/layout_store.hpp"
#include "../storage/learning_store.hpp"
#include "../storage/selection.hpp"
#include "../ui/candidate_window.hpp"
#include <stroke/dictionary/index.hpp>
#include <wrl/client.h>
#include <array>
#include <new>
#include <chrono>
#include <deque>

namespace stroke::win {
using Microsoft::WRL::ComPtr;
namespace {
Modifiers modifiers() noexcept {
    return {(GetKeyState(VK_SHIFT) & 0x8000) != 0, (GetKeyState(VK_CAPITAL) & 1) != 0,
        (GetKeyState(VK_CONTROL) & 0x8000) != 0, (GetKeyState(VK_MENU) & 0x8000) != 0,
        ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000) != 0};
}
WPARAM shift_key(WPARAM key, LPARAM flags) noexcept {
    if (key != VK_SHIFT) return key;
    return ((static_cast<ULONG_PTR>(flags) >> 16) & 0xff) == 0x36 ? VK_RSHIFT : VK_LSHIFT;
}
std::optional<wchar_t> english_character(WPARAM key, Modifiers mods, bool english) noexcept {
    if (const auto letter = english_letter(static_cast<unsigned int>(key), mods, english)) return letter;
    if (!english || mods.shortcut() || key > 255) return std::nullopt;
    BYTE keyboard[256]{};
    if (!GetKeyboardState(keyboard)) return std::nullopt;
    keyboard[VK_SHIFT] = keyboard[VK_LSHIFT] = mods.shift ? 0x80 : 0;
    keyboard[VK_RSHIFT] = 0;
    keyboard[VK_CAPITAL] = mods.caps_lock ? 1 : 0;
    keyboard[VK_CONTROL] = keyboard[VK_LCONTROL] = keyboard[VK_RCONTROL] = 0;
    keyboard[VK_MENU] = keyboard[VK_LMENU] = keyboard[VK_RMENU] = 0;
    keyboard[VK_LWIN] = keyboard[VK_RWIN] = 0;
    wchar_t text[8]{};
    const auto layout = GetKeyboardLayout(0);
    const auto count = ToUnicodeEx(static_cast<UINT>(key),
        MapVirtualKeyExW(static_cast<UINT>(key), MAPVK_VK_TO_VSC, layout), keyboard, text, 8,
        4, layout); // Windows 10 1607+: do not modify the keyboard's dead-key state.
    if (count == 1 && text[0] >= L' ' && text[0] <= L'~') return text[0];
    return std::nullopt;
}
class Edit final : public ITfEditSession {
public:
    explicit Edit(ITfContext* context) : context_(context) { ++live_objects; }
    ~Edit() { --live_objects; }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** result) override {
        if (!result) return E_POINTER;
        *result = nullptr;
        if (iid == IID_IUnknown || iid == IID_ITfEditSession) *result = static_cast<ITfEditSession*>(this);
        else return E_NOINTERFACE;
        AddRef(); return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override { const ULONG r = --refs_; if (!r) delete this; return r; }
    HRESULT STDMETHODCALLTYPE DoEditSession(TfEditCookie cookie) override {
        TF_SELECTION selection{};
        ULONG fetched{};
        HRESULT hr = context_->GetSelection(cookie, TF_DEFAULT_SELECTION, 1, &selection, &fetched);
        ComPtr<ITfRange> range;
        range.Attach(selection.range);
        if (FAILED(hr) || !fetched || !range) return E_FAIL;
        ComPtr<ITfContextView> view;
        hr = context_->GetActiveView(&view);
        if (FAILED(hr)) return hr;
        RECT rect{}; BOOL clipped{};
        hr = view->GetTextExt(cookie, range.Get(), &rect, &clipped);
        if (SUCCEEDED(hr)) { anchor = {rect.left, rect.bottom + 4}; positioned = true; }
        return hr;
    }
    POINT anchor{};
    bool positioned{};
private:
    std::atomic<ULONG> refs_{1};
    ComPtr<ITfContext> context_;
};

class Service final : public ITfTextInputProcessorEx, public ITfKeyEventSink,
                      public ITfThreadMgrEventSink, public ITfTextEditSink,
                      public ITfCompositionSink, public ITfDisplayAttributeProvider,
                      public ITfCompartmentEventSink {
public:
    explicit Service(std::filesystem::path storage_root = {}) : storage_root_(std::move(storage_root)), view_(module) { ++live_objects; }
    ~Service() { cleanup(); --live_objects; }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (iid == IID_IUnknown || iid == IID_ITfTextInputProcessor || iid == IID_ITfTextInputProcessorEx)
            *out = static_cast<ITfTextInputProcessorEx*>(this);
        else if (iid == IID_ITfKeyEventSink) *out = static_cast<ITfKeyEventSink*>(this);
        else if (iid == IID_ITfThreadMgrEventSink) *out = static_cast<ITfThreadMgrEventSink*>(this);
        else if (iid == IID_ITfTextEditSink) *out = static_cast<ITfTextEditSink*>(this);
        else if (iid == IID_ITfCompositionSink) *out = static_cast<ITfCompositionSink*>(this);
        else if (iid == IID_ITfDisplayAttributeProvider) *out = static_cast<ITfDisplayAttributeProvider*>(this);
        else if (iid == IID_ITfCompartmentEventSink) *out = static_cast<ITfCompartmentEventSink*>(this);
        else return E_NOINTERFACE;
        AddRef(); return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override { const ULONG r = --refs_; if (!r) delete this; return r; }
    HRESULT STDMETHODCALLTYPE EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo** out) override {
        if (!out) return E_POINTER; *out = new(std::nothrow) CompositionAttributes; return *out ? S_OK : E_OUTOFMEMORY;
    }
    HRESULT STDMETHODCALLTYPE GetDisplayAttributeInfo(REFGUID id, ITfDisplayAttributeInfo** out) override {
        if (!out) return E_POINTER; *out = nullptr;
        if (id != composition_attribute_id) return E_INVALIDARG;
        *out = new(std::nothrow) CompositionAttribute; return *out ? S_OK : E_OUTOFMEMORY;
    }
    HRESULT STDMETHODCALLTYPE OnChange(REFGUID id) override {
        if (id != GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION || publishing_mode_ || !mode_compartment_)
            return S_OK;
        try {
            VARIANT value; VariantInit(&value);
            const auto result = mode_compartment_->GetValue(&value);
            const bool valid = SUCCEEDED(result) && value.vt == VT_I4;
            const bool english = valid && !(value.lVal & TF_CONVERSIONMODE_NATIVE);
            VariantClear(&value);
            if (valid) request_mode(focused_context().Get(), english);
            return S_OK;
        } catch (...) { return E_FAIL; }
    }
    HRESULT STDMETHODCALLTYPE OnCompositionTerminated(TfEditCookie cookie, ITfComposition* composition) override {
        if (composition_.Get() == composition && !ending_composition_) {
            ComPtr<ITfRange> range; ComPtr<ITfProperty> property;
            if (context_ && SUCCEEDED(composition->GetRange(&range)) &&
                SUCCEEDED(context_->GetProperty(GUID_PROP_ATTRIBUTE, &property))) (void)property->Clear(cookie, range.Get());
            composition_.Reset(); rendered_.clear();
            reset();
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Activate(ITfThreadMgr* manager, TfClientId id) override { return ActivateEx(manager, id, 0); }
    HRESULT STDMETHODCALLTYPE ActivateEx(ITfThreadMgr* manager, TfClientId id, DWORD flags) override {
        if (!manager || id == TF_CLIENTID_NULL) return E_INVALIDARG;
        if (manager_) return E_UNEXPECTED;
        (void)flags; // UI visibility is negotiated through ITfUIElementMgr for all hosts.
        try {
            auto data = IndexedDictionary::load(module_path().parent_path() / L"dictionary" / L"dictionary.sidx");
            if (std::holds_alternative<Error>(data)) return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
            dictionary_ = std::get<std::shared_ptr<const IndexedDictionary>>(data);
            // A missing/corrupt/mismatched optional phrase file never disables basic input.
            auto phrases = PhraseIndex::load(module_path().parent_path() / L"dictionary" / L"phrases" / L"phrases.pidx");
            if (const auto* index = std::get_if<std::shared_ptr<const PhraseIndex>>(&phrases);
                index && (*index)->stroke_version() == dictionary_->info().data_version) phrases_ = *index;
            association_.configure(phrases_);
            if (storage_root_.empty()) (void)migrate_user_storage();
            settings_path_ = storage_root_.empty() ? layout_path() : storage_root_ / L"layout.dat";
            learning_path_ = storage_root_.empty() ? learning_path() : storage_root_ / L"learning.dat";
            poll_settings();
            applied_layout_ = pending_layout_;
            auto status = configure_session();
            if (std::holds_alternative<Error>(status)) return E_FAIL;
            manager_ = manager; client_ = id;
            initialize_mode();
            if (!manager_) return E_ABORT; // Shell item callbacks may deactivate reentrantly.
            ComPtr<ITfCategoryMgr> categories;
            if (SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&categories))))
                (void)categories->RegisterGUID(composition_attribute_id, &composition_atom_);
            HRESULT hr = manager_.As(&ui_manager_);
            if (FAILED(hr)) { cleanup(); return hr; }
            ComPtr<ITfKeystrokeMgr> keys;
            hr = manager_.As(&keys);
            if (SUCCEEDED(hr)) hr = keys->AdviseKeyEventSink(client_, this, TRUE);
            if (FAILED(hr)) { cleanup(); return hr; }
            keys_advised_ = true;
            ComPtr<ITfSource> source;
            hr = manager_.As(&source);
            if (SUCCEEDED(hr)) hr = source->AdviseSink(IID_ITfThreadMgrEventSink,
                static_cast<ITfThreadMgrEventSink*>(this), &manager_cookie_);
            if (FAILED(hr)) { cleanup(); return hr; }
            WNDCLASSW windowClass{};
            windowClass.lpfnWndProc = settings_proc;
            windowClass.hInstance = module;
            windowClass.lpszClassName = L"StrokeIME.SettingsWatcher";
            if (!RegisterClassW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) { cleanup(); return E_FAIL; }
            settings_window_ = CreateWindowExW(0, windowClass.lpszClassName, L"", 0, 0, 0, 0, 0,
                HWND_MESSAGE, nullptr, module, this);
            if (!settings_window_ || !SetTimer(settings_window_, 1, 500, nullptr)) { cleanup(); return E_FAIL; }
            return S_OK;
        } catch (...) { cleanup(); return E_FAIL; }
    }
    HRESULT STDMETHODCALLTYPE Deactivate() override { cleanup(); return S_OK; }
    HRESULT STDMETHODCALLTYPE OnSetFocus(BOOL foreground) override { if (!foreground) reset(); return S_OK; }
    HRESULT STDMETHODCALLTYPE OnTestKeyDown(ITfContext* context, WPARAM key, LPARAM, BOOL* eaten) override {
        if (!eaten) return E_POINTER;
        *eaten = FALSE;
        try {
            // Host-owned keys still invalidate an isolated Shift gesture.
            if (!shift_bit(static_cast<unsigned int>(key))) shift_tap_.down(static_cast<unsigned int>(key), modifiers());
            *eaten = handles(context, key, modifiers());
        } catch (...) { return E_FAIL; }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnTestKeyUp(ITfContext*, WPARAM key, LPARAM lp, BOOL* eaten) override {
        if (!eaten) return E_POINTER;
        if (const auto bit = shift_bit(static_cast<unsigned int>(shift_key(key, lp)))) {
            *eaten = (shift_swallowed_ & bit) != 0;
            return S_OK;
        }
        *eaten = key < swallowed_.size() && swallowed_[key]; return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnKeyUp(ITfContext* context, WPARAM key, LPARAM lp, BOOL* eaten) override {
        return key_up(context, key, lp, modifiers(), eaten);
    }
    HRESULT key_up(ITfContext* context, WPARAM key, LPARAM lp, Modifiers mods, BOOL* eaten) {
        const HRESULT hr = OnTestKeyUp(context, key, lp, eaten);
        if (FAILED(hr)) return hr;
        const auto physical = shift_key(key, lp);
        if (const auto bit = shift_bit(static_cast<unsigned int>(physical))) {
            shift_swallowed_ &= ~bit;
            const bool toggle = shift_tap_.up(static_cast<unsigned int>(physical));
            const bool same_context = shift_context_.Get() == context;
            if (!shift_tap_.held()) shift_context_.Reset();
            try {
                if (toggle && same_context && keyboard_enabled(context, mods) && focused_context().Get() == context)
                    request_mode(context, !effective_english());
            } catch (...) { return E_FAIL; }
            return S_OK;
        }
        if (key < swallowed_.size()) swallowed_[key] = false;
        return hr;
    }
    HRESULT STDMETHODCALLTYPE OnKeyDown(ITfContext* context, WPARAM key, LPARAM lp, BOOL* eaten) override {
        return key_down(context, shift_key(key, lp), modifiers(), eaten);
    }
    HRESULT key_down(ITfContext* context, WPARAM key, Modifiers mods, BOOL* eaten) {
        if (!eaten) return E_POINTER;
        *eaten = FALSE;
        try {
            if (!shift_bit(static_cast<unsigned int>(key))) shift_tap_.down(static_cast<unsigned int>(key), mods);
            if (!handles(context, key, mods)) {
                // A modifier press alone must not discard an unfinished stroke query.
                if (key != VK_SHIFT && key != VK_LSHIFT && key != VK_RSHIFT && key != VK_CAPITAL) reset();
                return S_OK;
            }
            if (const auto bit = shift_bit(static_cast<unsigned int>(key))) {
                if (!shift_tap_.held()) shift_context_ = context;
                shift_tap_.down(static_cast<unsigned int>(key), mods);
                shift_swallowed_ |= bit;
                *eaten = TRUE;
                return S_OK;
            }
            if (!bind(context)) return S_OK;
            *eaten = TRUE;
            if (key < swallowed_.size()) swallowed_[key] = true;
            if (pending_write_ || !queued_keys_.empty()) {
                if (key == VK_ESCAPE && !applied_layout_.continuous_input) { reset(); return S_OK; }
                // Repeated selection keys must not select the next composition accidentally.
                if (!effective_english() && !applied_layout_.continuous_input && queued_keys_.empty() && ((key >= '1' && key <= '9') || key == VK_SPACE || key == VK_RETURN)) return S_OK;
                if (queued_keys_.size() < 128) queued_keys_.push_back({key, mods});
            } else {
                process_key(context, key, mods);
            }
            return S_OK;
        } catch (...) {
            writing_ = false; reset(); return E_FAIL;
        }
    }
    void process_key(ITfContext* context, WPARAM key, Modifiers mods) {
            if (applied_layout_.continuous_input) { process_continuous(context, key, mods); return; }
            if (const auto letter = english_character(key, mods, english_mode_)) {
                learning_context_.clear();
                association_.reset(); association_anchor_.Reset();
                if (association_active()) session_.reset();
                insert(context, std::wstring(1, *letter), std::nullopt);
                return;
            }
            if (session_.snapshot().phase == SessionPhase::idle &&
                (pending_layout_ != applied_layout_ || learning_dirty_)) (void)configure_session();
            auto state = session_.snapshot();
            Result<Update> result = Error{ErrorCode::invalid_argument, "Unknown key"};
            if (key >= 'A' && key <= 'Z') {
                if (association_active()) session_.reset();
                result = session_.process_key(static_cast<char>(key-'A'+'a'));
            }
            else if (key == VK_BACK) result = session_.process(Backspace{});
            else if (key == VK_ESCAPE) { learning_context_.clear(); association_.reset(); association_anchor_.Reset(); result = session_.process(Cancel{}); }
            else if (key == VK_NEXT || key == VK_PRIOR || key == VK_RIGHT || key == VK_LEFT)
                result = session_.process(ChangePage{(key == VK_NEXT || key == VK_RIGHT)
                    ? PageDirection::next : PageDirection::previous});
            else {
                std::size_t index = 0;
                if (key >= '1' && key <= '9') index = candidate_index(static_cast<char>(key),
                    applied_layout_.reverse_selection, state.visible_candidates.size()).value_or(state.visible_candidates.size());
                if (index < state.visible_candidates.size()) result = session_.process(SelectCandidate{state.revision,index});
            }
            if (auto* update = std::get_if<Update>(&result); update && update->commit) {
                insert(context, utf16(update->commit->text), update->commit->revision, update->commit->text.front());
            }
            if (!pending_write_) show(context);
    }
    HRESULT STDMETHODCALLTYPE OnPreservedKey(ITfContext*, REFGUID, BOOL* eaten) override {
        if (!eaten) return E_POINTER; *eaten = FALSE; return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnInitDocumentMgr(ITfDocumentMgr*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnUninitDocumentMgr(ITfDocumentMgr*) override { reset(); return S_OK; }
    HRESULT STDMETHODCALLTYPE OnSetFocus(ITfDocumentMgr*, ITfDocumentMgr*) override { reset(); return S_OK; }
    HRESULT STDMETHODCALLTYPE OnPushContext(ITfContext*) override { reset(); return S_OK; }
    HRESULT STDMETHODCALLTYPE OnPopContext(ITfContext* context) override {
        if (context_.Get() == context) reset(); return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnEndEdit(ITfContext*, TfEditCookie, ITfEditRecord* record) override {
        if (writing_) { writing_ = false; return S_OK; }
        // Empty write sessions and property-only updates must not cancel a queued commit.
        BOOL changed = FALSE;
        bool invalid = !record || FAILED(record->GetSelectionStatus(&changed)) || changed;
        if (!invalid) {
            ComPtr<IEnumTfRanges> ranges;
            if (FAILED(record->GetTextAndPropertyUpdates(TF_GTP_INCL_TEXT, nullptr, 0, &ranges))) invalid = true;
            else if (ranges) {
                ComPtr<ITfRange> range;
                ULONG fetched{};
                const auto hr = ranges->Next(1, &range, &fetched);
                invalid = FAILED(hr) || fetched != 0;
            }
        }
        if (invalid) reset();
        return S_OK;
    }
private:
    ComPtr<ITfContext> focused_context() const {
        ComPtr<ITfDocumentMgr> document;
        ComPtr<ITfContext> context;
        if (manager_ && SUCCEEDED(manager_->GetFocus(&document)) && document)
            (void)document->GetTop(&context);
        return context;
    }
    bool effective_english() const noexcept {
        bool english = english_mode_;
        for (const auto& key : queued_keys_)
            if (key.mode.has_value()) english = *key.mode;
        return english;
    }
    bool keyboard_enabled(ITfContext* context, Modifiers mods) const {
        if (!manager_ || !context || mods.shortcut()) return false;
        TF_STATUS status{};
        return SUCCEEDED(context->GetStatus(&status)) && !(status.dwDynamicFlags & TS_SD_READONLY) &&
            !compartment(context, GUID_COMPARTMENT_KEYBOARD_DISABLED) &&
            !compartment(context, GUID_COMPARTMENT_EMPTYCONTEXT);
    }
    void publish_mode() noexcept {
        try {
            if (mode_compartment_ && !publishing_mode_) {
                const auto compartment = mode_compartment_;
                VARIANT value; VariantInit(&value);
                (void)compartment->GetValue(&value);
                const LONG flags = value.vt == VT_I4 ? value.lVal : 0;
                VariantClear(&value);
                value.vt = VT_I4;
                value.lVal = english_mode_ ? flags & ~TF_CONVERSIONMODE_NATIVE : flags | TF_CONVERSIONMODE_NATIVE;
                publishing_mode_ = true;
                (void)compartment->SetValue(client_, &value);
                publishing_mode_ = false;
            }
            const auto button = mode_button_;
            if (button) button->english(english_mode_);
        } catch (...) { publishing_mode_ = false; }
    }
    void initialize_mode() {
        const auto manager = manager_;
        ComPtr<ITfCompartmentMgr> compartments;
        if (SUCCEEDED(manager.As(&compartments)) &&
            SUCCEEDED(compartments->GetCompartment(GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION, &mode_compartment_))) {
            VARIANT value; VariantInit(&value);
            if (SUCCEEDED(mode_compartment_->GetValue(&value)) && value.vt == VT_I4)
                english_mode_ = !(value.lVal & TF_CONVERSIONMODE_NATIVE);
            VariantClear(&value);
            ComPtr<ITfSource> source;
            if (SUCCEEDED(mode_compartment_.As(&source)))
                (void)source->AdviseSink(IID_ITfCompartmentEventSink,
                    static_cast<ITfCompartmentEventSink*>(this), &mode_cookie_);
            publish_mode();
        }
        if (!manager_) return;
        ComPtr<ITfLangBarItemMgr> bar;
        if (SUCCEEDED(manager.As(&bar))) {
            ComPtr<InputModeButton> button;
            button.Attach(new InputModeButton(english_mode_, [this] {
                ComPtr<Service> self(this);
                const auto context = self->focused_context();
                self->request_mode(context.Get(), !self->effective_english());
            }));
            langbar_manager_ = bar;
            mode_button_ = button;
            const auto result = bar->AddItem(button.Get());
            if (mode_button_.Get() != button.Get()) {
                button->detach();
                if (SUCCEEDED(result)) (void)bar->RemoveItem(button.Get());
            } else if (FAILED(result)) {
                button->detach(); mode_button_.Reset(); langbar_manager_.Reset();
            }
        }
    }
    void cleanup_mode() noexcept {
        auto button = mode_button_; mode_button_.Reset();
        const auto bar = langbar_manager_; langbar_manager_.Reset();
        const auto compartment = mode_compartment_; mode_compartment_.Reset();
        const auto cookie = mode_cookie_; mode_cookie_ = TF_INVALID_COOKIE;
        if (button) button->detach();
        ComPtr<ITfSource> source;
        if (compartment && cookie != TF_INVALID_COOKIE && SUCCEEDED(compartment.As(&source)))
            (void)source->UnadviseSink(cookie);
        if (button && bar) (void)bar->RemoveItem(button.Get());
    }
    void apply_mode(ITfContext* context, bool english) {
        if (english_mode_ == english) return;
        if (applied_layout_.continuous_input) {
            const auto result = continuous_.action(ContinuousAction::discard_query);
            if (std::holds_alternative<Error>(result)) return;
        } else session_.reset();
        english_mode_ = english;
        learning_context_.clear(); association_.reset(); association_anchor_.Reset();
        end_ui();
        if (context && applied_layout_.continuous_input && (continuous_.active() || composition_)) {
            const auto text = continuous_.text();
            const auto caret = utf16(text.substr(0, continuous_.text_cursor())).size();
            sync_composition(context, utf16(text), static_cast<LONG>(caret), false);
        }
        publish_mode();
    }
    void request_mode(ITfContext* context, bool english) {
        if (!manager_ || effective_english() == english) return;
        if (!context) {
            reset();
            english_mode_ = english;
            publish_mode();
            return;
        }
        if (!keyboard_enabled(context, {}) || !bind(context)) return;
        if (pending_write_ || !queued_keys_.empty()) {
            if (queued_keys_.size() < 128) queued_keys_.push_back({0, {}, english});
            return;
        }
        apply_mode(context, english);
    }
    static LRESULT CALLBACK settings_proc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
        auto* self = reinterpret_cast<Service*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            self = static_cast<Service*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (message == WM_TIMER && self) { self->poll_settings(); self->drain(); return 0; }
        if (message == WM_APP + 1 && self) { self->drain(); return 0; }
        return DefWindowProcW(window, message, wp, lp);
    }
    void poll_settings() noexcept {
        try {
            auto loaded = load_layout(settings_path_);
            if (const auto* layout = std::get_if<Layout>(&loaded)) pending_layout_ = *layout;
            // Invalid/unreadable updates retain the last usable configuration.
        } catch (...) {}
        poll_learning();
    }
    void poll_learning() noexcept {
        try {
            auto result = sync_learning(learning_path_, learning_.generation, pending_uses_, pending_phrases_);
            if (const auto* state = std::get_if<LearningState>(&result)) {
                if (state->generation != learning_.generation) learning_context_.clear();
                if (state->phrases != learning_.phrases || state->enabled != learning_.enabled || personal_dirty_ || !personal_snapshot_) {
                    personal_snapshot_ = std::make_shared<const PersonalPhrases>(state->phrases);
                    personal_dirty_ = false;
                }
                association_.set_personal(personal_snapshot_, state->enabled);
                if (*state != learning_ || !learning_ready_) learning_dirty_ = true;
                learning_ = *state;
                pending_uses_.clear();
                pending_phrases_.clear();
                learning_ready_ = true;
            }
        } catch (...) {} // Learning failures never change the host insertion result.
    }
    Status configure_session() {
        auto config = layout_config(pending_layout_);
        config.learning_enabled = learning_ready_ && learning_.enabled;
        LearningCounts counts;
        if (config.learning_enabled) {
            for (const auto& [cp, use] : learning_.uses) counts.emplace(cp, use.count);
        }
        const auto result = session_.configure(dictionary_, config, counts);
        if (std::holds_alternative<Error>(result)) return result;
        const auto continuous_result = continuous_.configure(dictionary_, config, std::move(counts), phrases_);
        if (std::holds_alternative<Error>(continuous_result)) return continuous_result;
        if (std::holds_alternative<std::monostate>(result)) {
            applied_layout_ = pending_layout_;
            learning_dirty_ = false;
        }
        return result;
    }
    void remember(char32_t cp, const std::string& learning_generation, bool positioned = true) noexcept {
        try {
            if (!is_personal_character(cp)) { learning_context_.clear(); return; }
            if (!learning_ready_ || !learning_.enabled || learning_generation.empty() || learning_generation != learning_.generation) return;
            auto& increment = pending_uses_[cp];
            if (increment.count < 1000000) ++increment.count;
            increment.last_used = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
            auto& use = learning_.uses[cp];
            if (use.count < 1000000) ++use.count;
            use.last_used = increment.last_used;
            if (!positioned || !is_personal_character(cp)) learning_context_.clear();
            else {
                learning_context_ += cp;
                for (std::size_t length = 2; length <= std::min<std::size_t>(8, learning_context_.size()); ++length) {
                    const auto word = learning_context_.substr(learning_context_.size() - length);
                    if ((!learning_.phrases.contains(word) && learning_.phrases.size() >= personal_phrase_limit) ||
                        (!pending_phrases_.contains(word) && pending_phrases_.size() >= personal_phrase_limit)) continue;
                    auto& batch = pending_phrases_[word];
                    if (batch.count < 1000000) ++batch.count;
                    batch.last_used = increment.last_used;
                    auto& learned = learning_.phrases[word];
                    if (learned.count < 1000000) ++learned.count;
                    learned.last_used = increment.last_used;
                    personal_dirty_ = true;
                }
                if (learning_context_.size() > 7) learning_context_.erase(0, learning_context_.size() - 7);
            }
            learning_dirty_ = true;
        } catch (...) {}
    }
    bool valid_target(ITfContext* context, std::uint64_t generation) {
        if (!manager_ || generation != generation_ || context_.Get() != context) return false;
        ComPtr<ITfDocumentMgr> document;
        ComPtr<ITfContext> top;
        if (FAILED(manager_->GetFocus(&document)) || !document || FAILED(document->GetTop(&top)) || top.Get() != context) return false;
        TF_STATUS status{};
        return SUCCEEDED(context->GetStatus(&status)) && !(status.dwDynamicFlags & TS_SD_READONLY) &&
            !compartment(context, GUID_COMPARTMENT_KEYBOARD_DISABLED) && !compartment(context, GUID_COMPARTMENT_EMPTYCONTEXT);
    }
    bool association_active() const { return !session_.snapshot().association_prefix.empty(); }
    bool association_position(ITfContext* context, TfEditCookie cookie) {
        if (!association_anchor_) return false;
        TF_SELECTION selection{}; ULONG fetched{};
        const auto hr = context->GetSelection(cookie, TF_DEFAULT_SELECTION, 1, &selection, &fetched);
        ComPtr<ITfRange> range; range.Attach(selection.range);
        if (FAILED(hr) || fetched != 1 || !range) return false;
        LONG start{}, end{};
        return SUCCEEDED(range->CompareStart(cookie, association_anchor_.Get(), TF_ANCHOR_START, &start)) &&
               SUCCEEDED(range->CompareEnd(cookie, association_anchor_.Get(), TF_ANCHOR_START, &end)) &&
               start == 0 && end == 0;
    }
    void offer_association(char32_t cp) {
        if (effective_english() || !applied_layout_.association_input || applied_layout_.continuous_input || !association_anchor_) {
            association_.reset(); return;
        }
        try {
            auto candidates = association_.accepted(cp);
            if (!candidates.empty()) (void)session_.suggest(std::move(candidates), association_.context());
        } catch (...) { association_.reset(); session_.reset(); }
    }
    bool idle() const { return session_.snapshot().phase == SessionPhase::idle && !continuous_.active() && !composition_; }
    Session& query() { return applied_layout_.continuous_input ? continuous_.query() : session_; }
    void process_continuous(ITfContext* context, WPARAM key, Modifiers mods) {
        const bool selection_key = continuous_.choosing() && !mods.shift &&
            ((key >= '1' && key <= '9') || key == VK_SPACE);
        const auto letter = selection_key ? std::optional<wchar_t>{} : english_character(key, mods, english_mode_);
        if (letter && !continuous_.active() && !composition_) {
            learning_context_.clear(); insert(context, std::wstring(1, *letter), std::nullopt); return;
        }
        Result<ContinuousUpdate> result = ContinuousUpdate{};
        if (letter) {
            const bool has_draft = !continuous_.draft().empty();
            result = continuous_.literal(static_cast<char32_t>(*letter));
            // A stroke-only query is not a Chinese draft. Replace it with direct
            // English and finish in one edit, without leaving an English underline.
            if (!has_draft && !std::holds_alternative<Error>(result))
                result = continuous_.action(ContinuousAction::enter);
        } else if (key >= 'A' && key <= 'Z') result = continuous_.key(static_cast<char>(key-'A'+'a'));
        else if (key >= '1' && key <= '9') {
            const auto count = query().snapshot().visible_candidates.size();
            result = continuous_.select(candidate_index(static_cast<char>(key), applied_layout_.reverse_selection, count).value_or(count));
        } else if (key == VK_NEXT || key == VK_PRIOR) {
            if (!continuous_.choosing()) result = continuous_.action(ContinuousAction::candidates);
            if (!std::holds_alternative<Error>(result)) (void)query().process(ChangePage{key == VK_NEXT ? PageDirection::next : PageDirection::previous});
        } else {
            const auto action = key == VK_SPACE ? ContinuousAction::next_character :
                key == VK_RETURN ? ContinuousAction::enter : key == VK_LEFT ? ContinuousAction::left :
                key == VK_RIGHT ? ContinuousAction::right : key == VK_DOWN ? ContinuousAction::candidates :
                key == VK_BACK ? ContinuousAction::backspace : ContinuousAction::cancel;
            result = continuous_.action(action);
        }
        if (std::holds_alternative<Error>(result)) { MessageBeep(MB_OK); show(context); return; }
        auto update = std::get<ContinuousUpdate>(std::move(result));
        if (!update.consumed) { show(context); return; }
        auto text = utf16(continuous_.text());
        auto caret = utf16(continuous_.text().substr(0, continuous_.text_cursor())).size();
        if (update.commit) caret = text.size();
        sync_composition(context, std::move(text), static_cast<LONG>(caret), update.commit.has_value());
        if (!pending_write_) show(context);
    }
    HRESULT edit_composition(ITfContext* context, TfEditCookie cookie, const std::wstring& text,
                             LONG caret, bool finish, std::uint64_t generation) {
        if (!composition_) {
            if (text.empty()) return S_OK;
            ComPtr<ITfInsertAtSelection> inserter;
            ComPtr<ITfContextComposition> owner;
            HRESULT hr = context->QueryInterface(IID_PPV_ARGS(&inserter));
            if (FAILED(hr)) return hr;
            hr = context->QueryInterface(IID_PPV_ARGS(&owner)); if (FAILED(hr)) return hr;
            ComPtr<ITfRange> range;
            hr = inserter->InsertTextAtSelection(cookie, TF_IAS_QUERYONLY, L"", 0, &range);
            if (FAILED(hr) || !range) return FAILED(hr) ? hr : E_FAIL;
            ComPtr<ITfComposition> created;
            hr = owner->StartComposition(cookie, range.Get(), this, &created);
            if (FAILED(hr) || !created) return FAILED(hr) ? hr : E_FAIL;
            if (!valid_target(context, generation)) { (void)created->EndComposition(cookie); return TF_E_DISCONNECTED; }
            composition_ = created; rendered_.clear();
        }
        const auto current = composition_;
        ComPtr<ITfRange> range;
        HRESULT hr = current->GetRange(&range); if (FAILED(hr)) return hr;
        if (text != rendered_) {
            hr = range->SetText(cookie, 0, text.data(), static_cast<LONG>(text.size()));
            if (FAILED(hr)) return hr;
            (void)current->ShiftEnd(cookie, range.Get());
            rendered_ = text;
        }
        if (composition_.Get() != current.Get()) return TF_E_DISCONNECTED;
        ComPtr<ITfProperty> property;
        if (SUCCEEDED(context->GetProperty(GUID_PROP_ATTRIBUTE, &property))) {
            if (finish || text.empty()) (void)property->Clear(cookie, range.Get());
            else if (composition_atom_ != TF_INVALID_GUIDATOM) {
                VARIANT value{}; value.vt = VT_I4; value.lVal = static_cast<LONG>(composition_atom_);
                (void)property->SetValue(cookie, range.Get(), &value);
            }
        }
        ComPtr<ITfRange> position;
        if (SUCCEEDED(range->Clone(&position)) && SUCCEEDED(position->Collapse(cookie, TF_ANCHOR_START))) {
            LONG moved{};
            if (SUCCEEDED(position->ShiftEnd(cookie, caret, &moved, nullptr)) && moved == caret &&
                SUCCEEDED(position->Collapse(cookie, TF_ANCHOR_END))) {
                TF_SELECTION selection{position.Get(), {TF_AE_NONE, FALSE}};
                (void)context->SetSelection(cookie, 1, &selection);
            }
        }
        if (finish || text.empty()) {
            ending_composition_ = true;
            hr = current->EndComposition(cookie);
            ending_composition_ = false;
            if (SUCCEEDED(hr)) { composition_.Reset(); rendered_.clear(); }
            return hr;
        }
        return S_OK;
    }
    void sync_composition(ITfContext* context, std::wstring text, LONG caret, bool finish) {
        const auto generation = ++generation_;
        const auto learn_generation = learning_ready_ && learning_.enabled ? learning_.generation : std::string{};
        pending_write_ = true;
        ComPtr<Service> self(this);
        ComPtr<WriteSession> edit;
        edit.Attach(new WriteSession(context,
            [self, context, text = std::move(text), caret, finish, generation](TfEditCookie cookie) {
                return self->edit_composition(context, cookie, text, caret, finish, generation);
            },
            [self, context, generation] {
                if (!self->valid_target(context, generation)) return false;
                self->writing_ = true; return true;
            },
            [self, generation, finish, learn_generation](HRESULT hr) {
                if (generation != self->generation_) return;
                self->pending_write_ = false;
                self->composition_failed_ = FAILED(hr);
                if (FAILED(hr)) {
                    self->writing_ = false; self->queued_keys_.clear();
                    self->publish_mode();
                }
                if (finish) {
                    self->learning_context_.clear();
                    for (auto cp : self->continuous_.complete(SUCCEEDED(hr))) self->remember(cp, learn_generation);
                    self->learning_context_.clear();
                }
                self->refresh_due_ = true;
                if (self->settings_window_) PostMessageW(self->settings_window_, WM_APP + 1, 0, 0);
            }));
        edit->request(client_);
        if (!pending_write_) writing_ = false;
    }
    void detach_composition() noexcept {
        auto composition = composition_; composition_.Reset(); rendered_.clear();
        if (!composition || !context_) return;
        // End only this range; do not erase text after focus or an external edit changed it.
        // The callback owns no Service reference, avoiding a composition/sink/service cycle.
        try {
            const auto context = context_;
            ComPtr<WriteSession> edit;
            edit.Attach(new WriteSession(context.Get(),
                [composition, context](TfEditCookie cookie) {
                    ComPtr<ITfRange> range; ComPtr<ITfProperty> property;
                    if (SUCCEEDED(composition->GetRange(&range)) && SUCCEEDED(context->GetProperty(GUID_PROP_ATTRIBUTE, &property)))
                        (void)property->Clear(cookie, range.Get());
                    return composition->EndComposition(cookie);
                }, [] { return true; }, [](HRESULT) {}));
            edit->request(client_);
        } catch (...) {}
    }
    void insert(ITfContext* context, std::wstring text, std::optional<std::uint64_t> revision,
                std::optional<char32_t> committed = {}) {
        const bool associated = association_active();
        const auto generation = ++generation_;
        const auto learn_generation = learning_ready_ && learning_.enabled ? learning_.generation : std::string{};
        const auto positioned = std::make_shared<bool>(false);
        pending_write_ = true;
        // The queued callback keeps this COM object alive; reset invalidates its generation.
        ComPtr<Service> self(this);
        ComPtr<WriteSession> edit;
        edit.Attach(new WriteSession(context, std::move(text),
            [self, context, generation] {
                if (!self->valid_target(context, generation)) return false;
                self->writing_ = true; return true;
            },
            [self, generation, revision, committed, associated, learn_generation, positioned](HRESULT hr) {
                if (generation != self->generation_) return;
                if (FAILED(hr)) self->writing_ = false;
                self->pending_write_ = false;
                if (associated && hr == TF_E_DISCONNECTED) { self->reset(); return; }
                if (revision) {
                    const auto completed = self->session_.complete_commit(*revision, hr == S_OK);
                    if (std::holds_alternative<Update>(completed) && hr == S_OK && committed) {
                        self->remember(*committed, learn_generation, *positioned);
                        self->offer_association(*committed);
                    }
                } else if (hr == S_OK) self->session_.reset();
                if (hr != S_OK) { self->queued_keys_.clear(); self->publish_mode(); }
                self->refresh_due_ = true;
                if (self->settings_window_) PostMessageW(self->settings_window_, WM_APP + 1, 0, 0);
            },
            [self, positioned](ITfRange* range) {
                *positioned = range != nullptr;
                self->association_anchor_.Reset();
                if (range)
                    (void)range->Clone(&self->association_anchor_);
            },
            [self, context, associated](TfEditCookie cookie) {
                const bool same_position = self->association_position(context, cookie);
                if (!same_position) self->learning_context_.clear();
                return !associated || same_position;
            }));
        edit->request(client_);
        // Synchronous OnEndEdit is normally inside RequestEditSession; asynchronous completion
        // clears this in OnEndEdit or the posted drain, after the write lock has been released.
        if (!pending_write_) writing_ = false;
    }
    void drain() noexcept {
        if (pending_write_ || draining_) return;
        draining_ = true;
        writing_ = false;
        try {
            if (context_ && !valid_target(context_.Get(), generation_)) { reset(); draining_ = false; return; }
            while (!pending_write_ && !queued_keys_.empty() && context_) {
                if (!valid_target(context_.Get(), generation_)) { reset(); break; }
                const auto key = queued_keys_.front(); queued_keys_.pop_front();
                if (key.mode.has_value()) apply_mode(context_.Get(), *key.mode);
                else process_key(context_.Get(), key.key, key.mods);
            }
            if (refresh_due_ && context_) { refresh_due_ = false; show(context_.Get()); }
        } catch (...) { reset(); }
        draining_ = false;
    }
    static bool compartment(ITfContext* context, REFGUID id) noexcept {
        ComPtr<ITfCompartmentMgr> manager;
        ComPtr<ITfCompartment> value;
        if (FAILED(context->QueryInterface(IID_PPV_ARGS(&manager))) || FAILED(manager->GetCompartment(id, &value))) return false;
        VARIANT v; VariantInit(&v);
        const HRESULT hr = value->GetValue(&v);
        const bool result = SUCCEEDED(hr) && v.vt == VT_I4 && v.lVal != 0;
        VariantClear(&v); return result;
    }
    bool handles(ITfContext* context, WPARAM key, Modifiers mods) {
        if (!manager_ || !context) return false;
        if (pending_layout_ != applied_layout_ && association_active() && !pending_write_) {
            reset();
            if (!manager_) return false; // EndUIElement may re-enter Deactivate.
        }
        if ((pending_layout_ != applied_layout_ || learning_dirty_) && idle()) {
            (void)configure_session();
        }
        if (!keyboard_enabled(context, mods)) return false;
        if (shift_bit(static_cast<unsigned int>(key))) return true;
        const bool selection_key = applied_layout_.continuous_input && continuous_.choosing() && !mods.shift &&
            ((key >= '1' && key <= '9') || key == VK_SPACE);
        if (!selection_key && english_character(key, mods, effective_english())) return true;
        if (mods.shift) return false;
        if (key >= 'A' && key <= 'Z' && session_.handles_key(static_cast<char>(key-'A'+'a'))) return true;
        if (context_.Get() != context || (!pending_write_ && idle())) return false;
        if (association_active())
            return (key >= '1' && key <= '9') || key == VK_ESCAPE || key == VK_NEXT || key == VK_PRIOR;
        return (key >= '1' && key <= '9') || key == VK_SPACE || key == VK_RETURN
            || key == VK_ESCAPE || key == VK_BACK || key == VK_NEXT || key == VK_PRIOR
            || key == VK_LEFT || key == VK_RIGHT || (applied_layout_.continuous_input && key == VK_DOWN);
    }
    bool bind(ITfContext* context) {
        if (context_.Get() == context) return true;
        reset();
        ComPtr<ITfSource> source;
        if (FAILED(context->QueryInterface(IID_PPV_ARGS(&source)))) return false;
        if (FAILED(source->AdviseSink(IID_ITfTextEditSink, static_cast<ITfTextEditSink*>(this), &edit_cookie_))) return false;
        context_ = context; return true;
    }
    void show(ITfContext* context) {
        if (applied_layout_.continuous_input && !continuous_.choosing() && !composition_failed_) { end_ui(); return; }
        if (query().snapshot().phase == SessionPhase::idle) { end_ui(); return; }
        // Host UI callbacks may re-enter Deactivate/reset. Pin COM objects across those calls.
        ComPtr<ITfContext> target(context);
        const auto uiManager = ui_manager_;
        if (!uiManager) return;
        if (!element_) {
            ComPtr<ITfDocumentMgr> document;
            if (FAILED(context->GetDocumentMgr(&document))) return;
            element_.Attach(new CandidateElement(document.Get(),
                [this](bool showWindow) {
                    const bool changed = ui_allowed_ != showWindow;
                    ui_allowed_ = showWindow;
                    if (!showWindow) { view_.hide(); if (element_) element_->shown(false); }
                    else if (changed) { refresh_due_ = true; if (settings_window_) PostMessageW(settings_window_, WM_APP + 1, 0, 0); }
                },
                [this](std::vector<std::size_t> pages) -> HRESULT {
                    if (ui_allowed_) return E_INVALIDARG; // Native UI always uses its nine-key pages.
                    const auto revision = query().snapshot().revision;
                    const auto result = query().set_candidate_pages(std::move(pages));
                    if (std::holds_alternative<Error>(result)) return E_INVALIDARG;
                    if (query().snapshot().revision == revision) return S_OK;
                    if (element_) element_->update(query());
                    refresh_due_ = true;
                    if (settings_window_) PostMessageW(settings_window_, WM_APP + 1, 0, 0);
                    return S_OK;
                }));
            element_->update(query());
            const auto current = element_;
            BOOL allowed = TRUE;
            ui_allowed_ = false;
            const HRESULT hr = uiManager->BeginUIElement(element_.Get(), &allowed, &ui_id_);
            if (element_.Get() != current.Get()) {
                if (SUCCEEDED(hr)) (void)uiManager->EndUIElement(ui_id_);
                return;
            }
            if (FAILED(hr)) { element_->detach(); element_.Reset(); return; }
            ui_active_ = true; ui_allowed_ = allowed != FALSE;
        }
        if (ui_allowed_ && query().snapshot().phase == SessionPhase::composing && !query().candidates().empty()) {
            std::vector<std::size_t> pages;
            for (std::size_t i = 0; i < query().candidates().size(); i += 9) pages.push_back(i);
            (void)query().set_candidate_pages(std::move(pages));
        }
        element_->update(query());
        const auto current = element_;
        if (ui_active_) (void)uiManager->UpdateUIElement(ui_id_);
        // UpdateUIElement may re-enter Show(FALSE).
        if (element_.Get() != current.Get() || !ui_allowed_) { view_.hide(); return; }
        const auto snapshot = query().snapshot();
        POINT anchor{100,100};
        GUITHREADINFO gui{sizeof(gui)};
        if (GetGUIThreadInfo(GetCurrentThreadId(), &gui) && gui.hwndCaret) {
            anchor = {gui.rcCaret.left, gui.rcCaret.bottom+4}; ClientToScreen(gui.hwndCaret, &anchor);
        } else {
            ComPtr<ITfContextView> active;
            HWND hwnd{}; RECT rect{};
            if (SUCCEEDED(context->GetActiveView(&active)) && SUCCEEDED(active->GetWnd(&hwnd)) && hwnd && GetWindowRect(hwnd,&rect))
                anchor = {rect.left + 24, rect.top + 80};
        }
        ComPtr<Edit> edit; edit.Attach(new Edit(context));
        HRESULT outcome{};
        if (SUCCEEDED(context->RequestEditSession(client_, edit.Get(), TF_ES_SYNC | TF_ES_READ, &outcome))
            && SUCCEEDED(outcome) && edit->positioned) anchor = edit->anchor;
        ComPtr<ITfContextView> activeView;
        HWND owner{};
        if (SUCCEEDED(context->GetActiveView(&activeView))) (void)activeView->GetWnd(&owner);
        if (!owner) owner = GetFocus();
        view_.present(snapshot, anchor, owner, applied_layout_.reverse_selection);
        element_->shown(true);
    }
    void end_ui() noexcept {
        view_.hide();
        auto element = element_;
        element_.Reset();
        if (element) element->detach();
        const bool active = ui_active_; ui_active_ = false; ui_allowed_ = false;
        if (active && ui_manager_) (void)ui_manager_->EndUIElement(ui_id_);
    }
    void reset() noexcept {
        const bool requested_mode = effective_english();
        shift_tap_.cancel(); shift_context_.Reset();
        detach_composition();
        continuous_.reset(); composition_failed_ = false;
        ++generation_; pending_write_ = false; writing_ = false;
        queued_keys_.clear(); refresh_due_ = false;
        association_.reset(); association_anchor_.Reset();
        learning_context_.clear();
        session_.reset(); end_ui();
        if (context_ && edit_cookie_ != TF_INVALID_COOKIE) {
            ComPtr<ITfSource> source;
            if (SUCCEEDED(context_.As(&source))) (void)source->UnadviseSink(edit_cookie_);
        }
        edit_cookie_ = TF_INVALID_COOKIE; context_.Reset();
        if (english_mode_ != requested_mode) {
            english_mode_ = requested_mode;
            publish_mode();
        }
    }
    void cleanup() noexcept {
        if (learning_ready_ && (!pending_uses_.empty() || !pending_phrases_.empty())) poll_learning();
        if (settings_window_) { KillTimer(settings_window_, 1); DestroyWindow(settings_window_); settings_window_ = nullptr; }
        // Another service instance in the same module may still own a watcher.
        UnregisterClassW(L"StrokeIME.SettingsWatcher", module);
        cleanup_mode();
        reset(); swallowed_.fill(false);
        shift_swallowed_ = 0;
        english_mode_ = false;
        if (manager_) {
            ComPtr<ITfSource> source;
            if (manager_cookie_ != TF_INVALID_COOKIE && SUCCEEDED(manager_.As(&source))) (void)source->UnadviseSink(manager_cookie_);
            ComPtr<ITfKeystrokeMgr> keys;
            if (keys_advised_ && SUCCEEDED(manager_.As(&keys))) (void)keys->UnadviseKeyEventSink(client_);
        }
        manager_cookie_ = TF_INVALID_COOKIE; keys_advised_ = false;
        ui_manager_.Reset();
        manager_.Reset(); client_ = TF_CLIENTID_NULL;
        dictionary_.reset(); phrases_.reset();
        learning_ = {}; pending_uses_.clear(); pending_phrases_.clear();
        personal_snapshot_.reset(); personal_dirty_ = false;
        learning_ready_ = false; learning_dirty_ = true;
    }
    std::atomic<ULONG> refs_{1};
    ComPtr<ITfThreadMgr> manager_;
    ComPtr<ITfContext> context_;
    ComPtr<ITfUIElementMgr> ui_manager_;
    ComPtr<CandidateElement> element_;
    DWORD ui_id_{};
    bool ui_active_{}, ui_allowed_{};
    TfClientId client_{TF_CLIENTID_NULL};
    DWORD manager_cookie_{TF_INVALID_COOKIE}, edit_cookie_{TF_INVALID_COOKIE};
    bool keys_advised_{}, writing_{};
    struct QueuedKey { WPARAM key; Modifiers mods; std::optional<bool> mode{}; };
    std::deque<QueuedKey> queued_keys_;
    std::uint64_t generation_{};
    bool pending_write_{}, refresh_due_{}, draining_{};
    std::array<bool,256> swallowed_{};
    ShiftTap shift_tap_;
    ComPtr<ITfContext> shift_context_;
    unsigned shift_swallowed_{};
    bool english_mode_{}, publishing_mode_{};
    ComPtr<ITfCompartment> mode_compartment_;
    DWORD mode_cookie_{TF_INVALID_COOKIE};
    ComPtr<ITfLangBarItemMgr> langbar_manager_;
    ComPtr<InputModeButton> mode_button_;
    Session session_;
    ContinuousSession continuous_;
    Association association_;
    ComPtr<ITfRange> association_anchor_;
    ComPtr<ITfComposition> composition_;
    TfGuidAtom composition_atom_{TF_INVALID_GUIDATOM};
    std::wstring rendered_;
    bool ending_composition_{}, composition_failed_{};
    std::shared_ptr<const IndexedDictionary> dictionary_;
    std::shared_ptr<const PhraseIndex> phrases_;
    std::filesystem::path settings_path_;
    std::filesystem::path storage_root_;
    std::filesystem::path learning_path_;
    LearningState learning_;
    LearnedUses pending_uses_;
    PersonalPhrases pending_phrases_;
    std::shared_ptr<const PersonalPhrases> personal_snapshot_;
    std::u32string learning_context_;
    bool personal_dirty_{};
    bool learning_ready_{}, learning_dirty_{true};
    Layout applied_layout_, pending_layout_;
    HWND settings_window_{};
    CandidateWindow view_;
};
}
HRESULT create_service(REFIID iid, void** result) noexcept {
    if (!result) return E_POINTER;
    *result = nullptr;
    try { auto* service = new Service; const HRESULT hr = service->QueryInterface(iid,result); service->Release(); return hr; }
    catch (...) { return E_OUTOFMEMORY; }
}
}
