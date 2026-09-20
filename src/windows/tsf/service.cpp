#include "runtime.hpp"
#include "keyboard_policy.hpp"
#include "candidate_element.hpp"
#include "write_session.hpp"
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
                      public ITfThreadMgrEventSink, public ITfTextEditSink {
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
        else return E_NOINTERFACE;
        AddRef(); return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override { const ULONG r = --refs_; if (!r) delete this; return r; }
    HRESULT STDMETHODCALLTYPE Activate(ITfThreadMgr* manager, TfClientId id) override { return ActivateEx(manager, id, 0); }
    HRESULT STDMETHODCALLTYPE ActivateEx(ITfThreadMgr* manager, TfClientId id, DWORD flags) override {
        if (!manager || id == TF_CLIENTID_NULL) return E_INVALIDARG;
        if (manager_) return E_UNEXPECTED;
        (void)flags; // UI visibility is negotiated through ITfUIElementMgr for all hosts.
        try {
            auto data = IndexedDictionary::load(module_path().parent_path() / L"dictionary" / L"dictionary.sidx");
            if (std::holds_alternative<Error>(data)) return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
            dictionary_ = std::get<std::shared_ptr<const IndexedDictionary>>(data);
            settings_path_ = storage_root_.empty() ? layout_path() : storage_root_ / L"layout.dat";
            learning_path_ = storage_root_.empty() ? learning_path() : storage_root_ / L"learning.dat";
            poll_settings();
            applied_layout_ = pending_layout_;
            auto status = configure_session();
            if (std::holds_alternative<Error>(status)) return E_FAIL;
            manager_ = manager; client_ = id;
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
        try { *eaten = handles(context, key, modifiers()); } catch (...) { return E_FAIL; }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnTestKeyUp(ITfContext*, WPARAM key, LPARAM, BOOL* eaten) override {
        if (!eaten) return E_POINTER;
        *eaten = key < swallowed_.size() && swallowed_[key]; return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnKeyUp(ITfContext* context, WPARAM key, LPARAM lp, BOOL* eaten) override {
        const HRESULT hr = OnTestKeyUp(context, key, lp, eaten);
        if (key < swallowed_.size()) swallowed_[key] = false;
        return hr;
    }
    HRESULT STDMETHODCALLTYPE OnKeyDown(ITfContext* context, WPARAM key, LPARAM, BOOL* eaten) override {
        return key_down(context, key, modifiers(), eaten);
    }
    HRESULT key_down(ITfContext* context, WPARAM key, Modifiers mods, BOOL* eaten) {
        if (!eaten) return E_POINTER;
        *eaten = FALSE;
        try {
            if (!handles(context, key, mods)) {
                // A modifier press alone must not discard an unfinished stroke query.
                if (key != VK_SHIFT && key != VK_LSHIFT && key != VK_RSHIFT && key != VK_CAPITAL) reset();
                return S_OK;
            }
            if (!bind(context)) return S_OK;
            *eaten = TRUE;
            if (key < swallowed_.size()) swallowed_[key] = true;
            if (pending_write_ || !queued_keys_.empty()) {
                if (key == VK_ESCAPE) { reset(); return S_OK; }
                // Repeated selection keys must not select the next composition accidentally.
                if (queued_keys_.empty() && ((key >= '1' && key <= '9') || key == VK_SPACE || key == VK_RETURN)) return S_OK;
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
            if (const auto letter = english_letter(static_cast<unsigned int>(key), mods)) {
                insert(context, std::wstring(1, *letter), std::nullopt);
                return;
            }
            if (session_.snapshot().phase == SessionPhase::idle &&
                (pending_layout_ != applied_layout_ || learning_dirty_)) (void)configure_session();
            auto state = session_.snapshot();
            Result<Update> result = Error{ErrorCode::invalid_argument, "Unknown key"};
            if (key >= 'A' && key <= 'Z') result = session_.process_key(static_cast<char>(key-'A'+'a'));
            else if (key == VK_BACK) result = session_.process(Backspace{});
            else if (key == VK_ESCAPE) result = session_.process(Cancel{});
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
                insert(context, utf16(update->commit->text), update->commit->revision);
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
            auto result = sync_learning(learning_path_, learning_.generation, pending_uses_);
            if (const auto* state = std::get_if<LearningState>(&result)) {
                if (*state != learning_ || !learning_ready_) learning_dirty_ = true;
                learning_ = *state;
                pending_uses_.clear();
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
        const auto result = session_.configure(dictionary_, config, std::move(counts));
        if (std::holds_alternative<std::monostate>(result)) {
            applied_layout_ = pending_layout_;
            applied_generation_ = learning_.generation;
            learning_dirty_ = false;
        }
        return result;
    }
    void remember(char32_t cp) noexcept {
        try {
            if (!learning_ready_ || !learning_.enabled || applied_generation_ != learning_.generation) return;
            auto& increment = pending_uses_[cp];
            if (increment.count < 1000000) ++increment.count;
            increment.last_used = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
            auto& use = learning_.uses[cp];
            if (use.count < 1000000) ++use.count;
            use.last_used = increment.last_used;
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
    void insert(ITfContext* context, std::wstring text, std::optional<std::uint64_t> revision) {
        const auto generation = ++generation_;
        pending_write_ = true;
        // The queued callback keeps this COM object alive; reset invalidates its generation.
        ComPtr<Service> self(this);
        ComPtr<WriteSession> edit;
        edit.Attach(new WriteSession(context, std::move(text),
            [self, context, generation] {
                if (!self->valid_target(context, generation)) return false;
                self->writing_ = true; return true;
            },
            [self, generation, revision](HRESULT hr) {
                if (generation != self->generation_) return;
                if (FAILED(hr)) self->writing_ = false;
                self->pending_write_ = false;
                if (revision) {
                    const auto completed = self->session_.complete_commit(*revision, hr == S_OK);
                    if (const auto* done = std::get_if<Update>(&completed); done && done->learned_character)
                        self->remember(*done->learned_character);
                } else if (hr == S_OK) self->session_.reset();
                if (hr != S_OK) self->queued_keys_.clear();
                self->refresh_due_ = true;
                if (self->settings_window_) PostMessageW(self->settings_window_, WM_APP + 1, 0, 0);
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
                process_key(context_.Get(), key.key, key.mods);
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
        if ((pending_layout_ != applied_layout_ || learning_dirty_) && session_.snapshot().phase == SessionPhase::idle) {
            (void)configure_session();
        }
        if (mods.shortcut()) return false;
        TF_STATUS status{};
        if (FAILED(context->GetStatus(&status)) || (status.dwDynamicFlags & TS_SD_READONLY)) return false;
        if (compartment(context, GUID_COMPARTMENT_KEYBOARD_DISABLED) || compartment(context, GUID_COMPARTMENT_EMPTYCONTEXT)) return false;
        if (mods.shift) return english_letter(static_cast<unsigned int>(key), mods).has_value();
        if (key >= 'A' && key <= 'Z' && session_.handles_key(static_cast<char>(key-'A'+'a'))) return true;
        if (context_.Get() != context || (!pending_write_ && session_.snapshot().phase == SessionPhase::idle)) return false;
        return (key >= '1' && key <= '9') || key == VK_SPACE || key == VK_RETURN
            || key == VK_ESCAPE || key == VK_BACK || key == VK_NEXT || key == VK_PRIOR
            || key == VK_LEFT || key == VK_RIGHT;
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
        if (session_.snapshot().phase == SessionPhase::idle) { end_ui(); return; }
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
                    const auto revision = session_.snapshot().revision;
                    const auto result = session_.set_candidate_pages(std::move(pages));
                    if (std::holds_alternative<Error>(result)) return E_INVALIDARG;
                    if (session_.snapshot().revision == revision) return S_OK;
                    if (element_) element_->update(session_);
                    refresh_due_ = true;
                    if (settings_window_) PostMessageW(settings_window_, WM_APP + 1, 0, 0);
                    return S_OK;
                }));
            element_->update(session_);
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
        if (ui_allowed_ && session_.snapshot().phase == SessionPhase::composing && !session_.candidates().empty()) {
            std::vector<std::size_t> pages;
            for (std::size_t i = 0; i < session_.candidates().size(); i += 9) pages.push_back(i);
            (void)session_.set_candidate_pages(std::move(pages));
        }
        element_->update(session_);
        const auto current = element_;
        if (ui_active_) (void)uiManager->UpdateUIElement(ui_id_);
        // UpdateUIElement may re-enter Show(FALSE).
        if (element_.Get() != current.Get() || !ui_allowed_) { view_.hide(); return; }
        const auto snapshot = session_.snapshot();
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
        ++generation_; pending_write_ = false; writing_ = false;
        queued_keys_.clear(); refresh_due_ = false;
        session_.reset(); end_ui();
        if (context_ && edit_cookie_ != TF_INVALID_COOKIE) {
            ComPtr<ITfSource> source;
            if (SUCCEEDED(context_.As(&source))) (void)source->UnadviseSink(edit_cookie_);
        }
        edit_cookie_ = TF_INVALID_COOKIE; context_.Reset();
    }
    void cleanup() noexcept {
        if (learning_ready_ && !pending_uses_.empty()) poll_learning();
        if (settings_window_) { KillTimer(settings_window_, 1); DestroyWindow(settings_window_); settings_window_ = nullptr; }
        // Another service instance in the same module may still own a watcher.
        UnregisterClassW(L"StrokeIME.SettingsWatcher", module);
        reset(); swallowed_.fill(false);
        if (manager_) {
            ComPtr<ITfSource> source;
            if (manager_cookie_ != TF_INVALID_COOKIE && SUCCEEDED(manager_.As(&source))) (void)source->UnadviseSink(manager_cookie_);
            ComPtr<ITfKeystrokeMgr> keys;
            if (keys_advised_ && SUCCEEDED(manager_.As(&keys))) (void)keys->UnadviseKeyEventSink(client_);
        }
        manager_cookie_ = TF_INVALID_COOKIE; keys_advised_ = false;
        ui_manager_.Reset();
        manager_.Reset(); client_ = TF_CLIENTID_NULL;
        dictionary_.reset();
        learning_ = {}; pending_uses_.clear(); applied_generation_.clear();
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
    struct QueuedKey { WPARAM key; Modifiers mods; };
    std::deque<QueuedKey> queued_keys_;
    std::uint64_t generation_{};
    bool pending_write_{}, refresh_due_{}, draining_{};
    std::array<bool,256> swallowed_{};
    Session session_;
    std::shared_ptr<const IndexedDictionary> dictionary_;
    std::filesystem::path settings_path_;
    std::filesystem::path storage_root_;
    std::filesystem::path learning_path_;
    LearningState learning_;
    LearnedUses pending_uses_;
    std::string applied_generation_;
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
