// Include the concrete adapter to exercise the same implementation with isolated storage.
#include "../../src/windows/tsf/service.cpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace stroke::win {
HINSTANCE module = GetModuleHandleW(nullptr);
std::atomic<long> live_objects{};
std::filesystem::path fixture_module;
std::filesystem::path module_path() { return fixture_module; }
}
using namespace stroke;
using namespace stroke::win;
using Microsoft::WRL::ComPtr;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
#define STUB(name, args) HRESULT STDMETHODCALLTYPE name args override { return E_NOTIMPL; }
#define REFS ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; } \
    ULONG STDMETHODCALLTYPE Release() override { return --refs; } ULONG refs{1}

struct EmptyEdit final : ITfEditRecord {
    REFS;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (!out) return E_POINTER; *out = nullptr;
        if (iid != IID_IUnknown && iid != IID_ITfEditRecord) return E_NOINTERFACE;
        *out = static_cast<ITfEditRecord*>(this); AddRef(); return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetSelectionStatus(BOOL* changed) override { *changed = FALSE; return S_OK; }
    HRESULT STDMETHODCALLTYPE GetTextAndPropertyUpdates(DWORD, const GUID**, ULONG, IEnumTfRanges** out) override {
        *out = nullptr; return S_OK;
    }
};
#include "tsf_composition_fixture.hpp"
struct Context final : ITfContext, ITfInsertAtSelection, ITfSource, ITfContextComposition {
    REFS;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (!out) return E_POINTER; *out = nullptr;
        if (iid == IID_IUnknown || iid == IID_ITfContext) *out = static_cast<ITfContext*>(this);
        else if (iid == IID_ITfInsertAtSelection) *out = static_cast<ITfInsertAtSelection*>(this);
        else if (iid == IID_ITfSource) *out = static_cast<ITfSource*>(this);
        else if (iid == IID_ITfContextComposition) *out = static_cast<ITfContextComposition*>(this);
        else return E_NOINTERFACE;
        AddRef(); return S_OK;
    }
    bool async{}, refuse{}, fail_insert{}, readonly{}, reject_composition{}, fail_end{}, fail_caret{};
    LONG caret{};
    int starts{}, finishes{};
    ComPtr<ITfComposition> active_composition;
    HRESULT STDMETHODCALLTYPE StartComposition(TfEditCookie, ITfRange* range, ITfCompositionSink* composition_sink, ITfComposition** out) override {
        *out = nullptr;
        if (reject_composition) return S_OK;
        auto* created = new HostComposition;
        created->range = range; created->sink = composition_sink; created->fail_end = &fail_end;
        created->on_end = [this] { ++finishes; active_composition.Reset(); };
        active_composition.Attach(created); ++starts;
        return active_composition.CopyTo(out);
    }
    STUB(EnumCompositions, (IEnumITfCompositionView**))
    STUB(FindComposition, (TfEditCookie, ITfRange*, IEnumITfCompositionView**))
    STUB(TakeOwnership, (TfEditCookie, ITfCompositionView*, ITfCompositionSink*, ITfComposition**))
    int sync_requests{}, async_requests{}, insertions{};
    ITfDocumentMgr* document{};
    std::wstring text;
    ComPtr<ITfEditSession> queued;
    ComPtr<ITfTextEditSink> sink;
    HRESULT run(ITfEditSession* edit) {
        const auto hr = edit->DoEditSession(1);
        if (sink && hr == S_OK) (void)sink->OnEndEdit(this, 1, nullptr);
        else if (sink) { EmptyEdit empty; (void)sink->OnEndEdit(this, 1, &empty); }
        return hr;
    }
    HRESULT flush() { auto edit = queued; queued.Reset(); return edit ? run(edit.Get()) : E_UNEXPECTED; }
    HRESULT STDMETHODCALLTYPE RequestEditSession(TfClientId, ITfEditSession* edit, DWORD flags, HRESULT* out) override {
        if (!(flags & TF_ES_READWRITE)) { *out = E_FAIL; return S_OK; }
        if (flags & TF_ES_SYNC) {
            ++sync_requests;
            if (async) { *out = TF_E_SYNCHRONOUS; return S_OK; }
            *out = run(edit); return S_OK;
        }
        ++async_requests;
        if (refuse) { *out = TS_E_READONLY; return S_OK; }
        queued = edit; *out = TF_S_ASYNC; return S_OK;
    }
    HRESULT STDMETHODCALLTYPE InsertTextAtSelection(TfEditCookie, DWORD flags, const WCHAR* value, LONG count, ITfRange** range) override {
        *range = nullptr;
        if (flags & TF_IAS_QUERYONLY) { *range = new CompositionRange(&text,&fail_insert,caret,caret); return S_OK; }
        if (fail_insert) return E_FAIL;
        ++insertions;
        text.insert(static_cast<std::size_t>(caret),value,static_cast<std::size_t>(count));
        *range = new CompositionRange(&text,&fail_insert,caret,caret+count); return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetStatus(TF_STATUS* out) override { *out = {}; out->dwDynamicFlags = readonly ? TS_SD_READONLY : 0; return S_OK; }
    HRESULT STDMETHODCALLTYPE GetDocumentMgr(ITfDocumentMgr** out) override { *out = document; if (*out) (*out)->AddRef(); return S_OK; }
    HRESULT STDMETHODCALLTYPE AdviseSink(REFIID iid, IUnknown* value, DWORD* cookie) override {
        if (iid != IID_ITfTextEditSink) return E_NOINTERFACE;
        *cookie = 1; return value->QueryInterface(IID_PPV_ARGS(&sink));
    }
    HRESULT STDMETHODCALLTYPE UnadviseSink(DWORD) override { sink.Reset(); return S_OK; }
    STUB(InWriteSession, (TfClientId, BOOL*))
    HRESULT STDMETHODCALLTYPE GetSelection(TfEditCookie, ULONG, ULONG count, TF_SELECTION* selected, ULONG* fetched) override {
        if (count != 1) return E_INVALIDARG;
        selected[0] = {new CompositionRange(&text,&fail_insert,caret,caret),{TF_AE_NONE,FALSE}};
        *fetched = 1; return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetSelection(TfEditCookie, ULONG count, const TF_SELECTION* selected) override {
        if (fail_caret) return E_FAIL;
        if (count != 1) return E_INVALIDARG;
        caret = static_cast<CompositionRange*>(selected->range)->start; return S_OK;
    }
    STUB(GetStart, (TfEditCookie, ITfRange**))
    STUB(GetEnd, (TfEditCookie, ITfRange**))
    STUB(GetActiveView, (ITfContextView**))
    STUB(EnumViews, (IEnumTfContextViews**))
    STUB(GetProperty, (REFGUID, ITfProperty**))
    STUB(GetAppProperty, (REFGUID, ITfReadOnlyProperty**))
    STUB(TrackProperties, (const GUID**, ULONG, const GUID**, ULONG, ITfReadOnlyProperty**))
    STUB(EnumProperties, (IEnumTfProperties**))
    STUB(CreateRangeBackup, (TfEditCookie, ITfRange*, ITfRangeBackup**))
    STUB(InsertEmbeddedAtSelection, (TfEditCookie, DWORD, IDataObject*, ITfRange**))
};
struct Document final : ITfDocumentMgr {
    REFS;
    ITfContext* top{};
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (!out) return E_POINTER; *out = nullptr;
        if (iid != IID_IUnknown && iid != IID_ITfDocumentMgr) return E_NOINTERFACE;
        *out = static_cast<ITfDocumentMgr*>(this); AddRef(); return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetTop(ITfContext** out) override { *out = top; if (top) top->AddRef(); return S_OK; }
    HRESULT STDMETHODCALLTYPE GetBase(ITfContext** out) override { return GetTop(out); }
    STUB(CreateContext, (TfClientId, DWORD, IUnknown*, ITfContext**, TfEditCookie*))
    STUB(Push, (ITfContext*))
    STUB(Pop, (DWORD))
    STUB(EnumContexts, (IEnumTfContexts**))
};
struct Manager final : ITfThreadMgr, ITfUIElementMgr, ITfKeystrokeMgr, ITfSource {
    REFS;
    ITfDocumentMgr* focus{};
    ComPtr<ITfKeyEventSink> keys;
    ComPtr<ITfThreadMgrEventSink> events;
    ComPtr<ITfUIElement> element;
    bool repeat_show{}, native_ui{};
    std::function<void()> on_begin;
    int begins{}, updates{}, ends{};
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (!out) return E_POINTER; *out = nullptr;
        if (iid == IID_IUnknown || iid == IID_ITfThreadMgr) *out = static_cast<ITfThreadMgr*>(this);
        else if (iid == IID_ITfUIElementMgr) *out = static_cast<ITfUIElementMgr*>(this);
        else if (iid == IID_ITfKeystrokeMgr) *out = static_cast<ITfKeystrokeMgr*>(this);
        else if (iid == IID_ITfSource) *out = static_cast<ITfSource*>(this);
        else return E_NOINTERFACE;
        AddRef(); return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetFocus(ITfDocumentMgr** out) override { *out = focus; if (focus) focus->AddRef(); return S_OK; }
    HRESULT STDMETHODCALLTYPE BeginUIElement(ITfUIElement* value, BOOL* show, DWORD* id) override {
        ++begins; element = value; *show = native_ui ? TRUE : FALSE; *id = 7;
        if (on_begin) on_begin();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE UpdateUIElement(DWORD id) override {
        check(id == 7, "UI identity"); ++updates;
        if (repeat_show && element) (void)element->Show(TRUE);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE EndUIElement(DWORD id) override { check(id == 7, "End identity"); ++ends; element.Reset(); return S_OK; }
    HRESULT STDMETHODCALLTYPE GetUIElement(DWORD, ITfUIElement** out) override { return element.CopyTo(out); }
    HRESULT STDMETHODCALLTYPE AdviseKeyEventSink(TfClientId, ITfKeyEventSink* value, BOOL) override { keys = value; return S_OK; }
    HRESULT STDMETHODCALLTYPE UnadviseKeyEventSink(TfClientId) override { keys.Reset(); return S_OK; }
    HRESULT STDMETHODCALLTYPE AdviseSink(REFIID iid, IUnknown* value, DWORD* cookie) override {
        if (iid != IID_ITfThreadMgrEventSink) return E_NOINTERFACE;
        *cookie = 1; return value->QueryInterface(IID_PPV_ARGS(&events));
    }
    HRESULT STDMETHODCALLTYPE UnadviseSink(DWORD) override { events.Reset(); return S_OK; }
    STUB(Activate, (TfClientId*))
    STUB(Deactivate, ())
    STUB(CreateDocumentMgr, (ITfDocumentMgr**))
    STUB(EnumDocumentMgrs, (IEnumTfDocumentMgrs**))
    STUB(SetFocus, (ITfDocumentMgr*))
    STUB(AssociateFocus, (HWND, ITfDocumentMgr*, ITfDocumentMgr**))
    STUB(IsThreadFocus, (BOOL*))
    STUB(GetFunctionProvider, (REFCLSID, ITfFunctionProvider**))
    STUB(EnumFunctionProviders, (IEnumTfFunctionProviders**))
    STUB(GetGlobalCompartment, (ITfCompartmentMgr**))
    STUB(EnumUIElements, (IEnumTfUIElements**))
    STUB(GetForeground, (CLSID*))
    STUB(TestKeyDown, (WPARAM, LPARAM, BOOL*))
    STUB(TestKeyUp, (WPARAM, LPARAM, BOOL*))
    STUB(KeyDown, (WPARAM, LPARAM, BOOL*))
    STUB(KeyUp, (WPARAM, LPARAM, BOOL*))
    STUB(GetPreservedKey, (ITfContext*, const TF_PRESERVEDKEY*, GUID*))
    STUB(IsPreservedKey, (REFGUID, const TF_PRESERVEDKEY*, BOOL*))
    STUB(PreserveKey, (TfClientId, REFGUID, const TF_PRESERVEDKEY*, const WCHAR*, ULONG))
    STUB(UnpreserveKey, (REFGUID, const TF_PRESERVEDKEY*))
    STUB(SetPreservedKeyDescription, (REFGUID, const WCHAR*, ULONG))
    STUB(GetPreservedKeyDescription, (REFGUID, BSTR*))
    STUB(SimulatePreservedKey, (ITfContext*, REFGUID, BOOL*))
};
#undef STUB
#undef REFS
void pump() {
    MSG message{};
    int budget = 1000;
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        check(--budget > 0, "UI notifications must not spin");
        DispatchMessageW(&message);
    }
}
void key(Manager& manager, Context& context, WPARAM code) {
    BOOL eaten{};
    const auto keys = manager.keys;
    auto* service = static_cast<Service*>(keys.Get());
    check(service->key_down(&context, code, {}, &eaten) == S_OK && eaten, "Key consumed");
    (void)keys->OnKeyUp(&context, code, 0, &eaten);
}
int wmain(int argc, wchar_t** argv) {
    try {
        check(argc == 2, "Dictionary fixture DLL required");
        const std::filesystem::path original_module = argv[1];
        const auto root = std::filesystem::current_path() / (L"tsf-host-" + std::to_wstring(GetCurrentProcessId()));
        std::filesystem::create_directories(root);
        // The basic composition tests must not depend on an optional packaged lexicon.
        fixture_module = root / L"basic-host" / L"stroke_tsf.dll";
        const auto base_bundle = fixture_module.parent_path() / L"dictionary";
        std::filesystem::create_directories(base_bundle);
        std::filesystem::copy_file(original_module.parent_path() / L"dictionary" / L"dictionary.sidx",
            base_bundle / L"dictionary.sidx", std::filesystem::copy_options::overwrite_existing);
        const auto frequency = original_module.parent_path() / L"dictionary" / L"frequency";
        if (std::filesystem::exists(frequency))
            std::filesystem::copy(frequency, base_bundle / L"frequency",
                std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing);
        Context context;
        Document document; document.top = &context; context.document = &document;
        Manager manager; manager.focus = &document;
        ComPtr<Service> service; service.Attach(new Service(root));
        check(service->ActivateEx(&manager, 1, TF_TMAE_UIELEMENTENABLEDONLY) == S_OK, "UILess activation");
        for (WPARAM arrow : {WPARAM(VK_LEFT), WPARAM(VK_RIGHT)}) {
            BOOL eaten = TRUE;
            check(service->key_down(&context, arrow, {}, &eaten) == S_OK && !eaten,
                "Idle arrows belong to host");
        }
        // Direct adapter callbacks use captured modifiers and never inject physical keys.
        key(manager, context, 'A');
        check(manager.begins >= 1 && manager.element, "Candidate registered");
        ComPtr<ITfCandidateListUIElement> list;
        check(manager.element.As(&list) == S_OK, "Host can query candidates");
        UINT count{}, pages{}, selected{};
        check(list->GetCount(&count) == S_OK && count > 9, "Full candidate list, not just visible page");
        BOOL shown = TRUE;
        check(list->IsShown(&shown) == S_OK && !shown, "Host veto suppresses native window");
        manager.repeat_show = true;
        check(list->Show(TRUE) == S_OK, "Host can permit native UI at runtime"); pump();
        check(list->IsShown(&shown) == S_OK && shown, "Native UI is actually shown");
        manager.repeat_show = false;
        check(list->Show(FALSE) == S_OK && list->IsShown(&shown) == S_OK && !shown, "Host can hide native UI");
        BSTR value{};
        check(list->GetString(0, &value) == S_OK && std::wstring(value) == L"一", "First candidate"); SysFreeString(value);
        check(list->GetString(count, &value) == E_INVALIDARG && !value, "Candidate bounds");
        check(list->GetCount(nullptr) == E_POINTER, "COM null checks");
        check(list->GetPageIndex(nullptr, 0, &pages) == S_FALSE && pages > 1, "Page sizing query");
        UINT custom[]{0, 3, 7};
        check(list->SetPageIndex(custom, 3) == S_OK, "Host pagination"); pump();
        key(manager, context, VK_NEXT);
        check(list->GetSelection(&selected) == S_OK && selected == 3, "Host page used by keyboard");
        key(manager, context, VK_LEFT);
        check(list->GetSelection(&selected) == S_OK && selected == 0, "Left arrow previous page");
        key(manager, context, VK_RIGHT);
        check(list->GetSelection(&selected) == S_OK && selected == 3, "Right arrow next page");
        key(manager, context, VK_RIGHT);
        check(list->GetSelection(&selected) == S_OK && selected == 7, "Right arrow uses host page boundaries");
        key(manager, context, VK_PRIOR);
        check(list->GetSelection(&selected) == S_OK && selected == 3, "PageUp still supported");
        UINT invalid[]{0,0}; check(list->SetPageIndex(invalid, 2) == E_INVALIDARG, "Reject invalid pages");
        key(manager, context, VK_ESCAPE);
        check(list->Show(TRUE) == TF_E_DISCONNECTED, "Retained element cannot call dead composition");
        list.Reset();
        for (const Modifiers mods : {Modifiers{true}, Modifiers{false,false,true},
                Modifiers{false,false,false,true}, Modifiers{false,false,false,false,true}}) {
            for (WPARAM arrow : {WPARAM(VK_LEFT), WPARAM(VK_RIGHT)}) {
                key(manager, context, 'A');
                BOOL eaten = TRUE;
                check(service->key_down(&context, arrow, mods, &eaten) == S_OK && !eaten,
                    "Modified arrows remain host-owned during composition");
            }
        }

        key(manager, context, 'A'); key(manager, context, '1'); pump();
        check(context.text == L"一" && context.async_requests == 0, "Synchronous fast path");
        context.async = true;
        key(manager, context, 'A'); key(manager, context, '1');
        check(context.queued && context.text == L"一", "No early asynchronous insertion");
        EmptyEdit empty;
        check(context.sink->OnEndEdit(&context, 1, &empty) == S_OK, "Empty edit notification");
        key(manager, context, '1'); // repeated selection must not duplicate
        key(manager, context, 'A'); key(manager, context, '1'); // next complete input queued
        auto duplicate = context.queued;
        check(context.flush() == S_OK, "Delayed commit succeeds"); pump();
        check(duplicate->DoEditSession(1) == E_UNEXPECTED, "Duplicate host callback never inserts twice"); duplicate.Reset();
        check(context.queued && context.text == L"一一", "Queued keys retain order");
        check(context.flush() == S_OK, "Second queued commit"); pump();
        check(context.text == L"一一一", "Exactly one insert per selection");

        key(manager, context, 'A'); key(manager, context, '1'); key(manager, context, VK_ESCAPE);
        check(context.flush() == TF_E_DISCONNECTED && context.text == L"一一一", "Escape invalidates queued write"); pump();
        key(manager, context, 'A'); key(manager, context, '1');
        service->OnSetFocus(FALSE);
        check(context.flush() == TF_E_DISCONNECTED, "Focus event invalidates queued write"); pump();
        key(manager, context, 'A'); key(manager, context, '1');
        manager.focus = nullptr;
        check(context.flush() == TF_E_DISCONNECTED, "Recheck actual focus even without notification"); pump(); manager.focus = &document;
        key(manager, context, 'A'); key(manager, context, '1');
        context.readonly = true;
        check(context.flush() == TF_E_DISCONNECTED, "Readonly transition blocks queued write"); pump(); context.readonly = false;
        service->OnSetFocus(FALSE);
        key(manager, context, 'A'); key(manager, context, '1');
        check(context.sink->OnEndEdit(&context, 1, nullptr) == S_OK, "External text/selection change");
        check(context.flush() == TF_E_DISCONNECTED, "External edit invalidates queued write"); pump();

        context.fail_insert = true;
        key(manager, context, 'A'); key(manager, context, '1');
        check(context.flush() == E_FAIL, "Insertion error"); pump();
        check(manager.element != nullptr, "Failure retains candidates");
        context.fail_insert = false;
        key(manager, context, '1'); check(context.flush() == S_OK, "Retry restored candidate"); pump();
        context.refuse = true;
        key(manager, context, 'A'); key(manager, context, '1'); pump();
        check(!context.queued && manager.element, "Refused async request restores composition");
        context.refuse = false; key(manager, context, VK_ESCAPE);
        BOOL eaten{};
        check(service->key_down(&context, 'B', {.shift=true}, &eaten) == S_OK && eaten && context.queued,
            "Shift English uses async path");
        check(service->key_down(&context, 'C', {.shift=true, .caps_lock=true}, &eaten) == S_OK && eaten,
            "Captured Caps Lock for queued English");
        check(context.flush() == S_OK, "Lowercase English commit"); pump();
        check(context.flush() == S_OK, "Uppercase queued English commit"); pump();
        check(context.text.ends_with(L"bC"), "English preserves captured modifiers and ordering");
        key(manager, context, 'A'); key(manager, context, '1');
        check(service->Deactivate() == S_OK, "Deactivate while pending");
        service.Reset();
        check(context.flush() == TF_E_DISCONNECTED, "Late callback after deactivation is harmless"); pump();
        check(live_objects == 0 && manager.begins == manager.ends, "No live COM objects or unended UI elements");
        auto learned = sync_learning(root / L"learning.dat");
        check(std::holds_alternative<LearningState>(learned), "Learning persisted");
        check(std::get<LearningState>(learned).uses.at(U'一').count == 4, "Only four successful writes learned");
        service.Attach(new Service(root));
        check(service->ActivateEx(&manager, 1, 0) == S_OK, "Normal activation also negotiates UI");
        manager.on_begin = [&] { check(service->Deactivate() == S_OK, "Reentrant deactivation"); };
        key(manager, context, 'A');
        manager.on_begin = {}; service.Reset();
        check(live_objects == 0 && manager.begins == manager.ends, "Reentrant Begin still pairs End and releases objects");
        // Continuous mode: edit an owned range, never insert the draft twice.
        Layout continuous_layout; continuous_layout.continuous_input = true;
        check(std::holds_alternative<std::monostate>(save_layout(root/L"layout.dat", continuous_layout)), "Enable continuous");
        context.async = false; context.text = L"prefix:"; context.caret = static_cast<LONG>(context.text.size());
        service.Attach(new Service(root));
        check(service->ActivateEx(&manager, 1, 0) == S_OK, "Continuous activation");
        ComPtr<ITfDisplayAttributeProvider> attributes;
        check(service.As(&attributes) == S_OK, "Composition attributes exposed");
        ComPtr<ITfDisplayAttributeInfo> info;
        check(attributes->GetDisplayAttributeInfo(composition_attribute_id,&info) == S_OK, "Display attribute identity");
        TF_DISPLAYATTRIBUTE style{}; check(info->GetAttributeInfo(&style) == S_OK && style.lsStyle == TF_LS_SOLID, "Underline attribute");
        info.Reset(); attributes.Reset();
        key(manager,context,'A'); key(manager,context,VK_SPACE);
        key(manager,context,'A'); key(manager,context,'A');
        check(context.text == L"prefix:一一一" && context.caret == 10 && context.active_composition,
            "Pending multi-stroke query stays raw and advances the composition caret");
        key(manager,context,VK_BACK);
        check(context.text == L"prefix:一一" && context.caret == 9,
            "Backspace shortens only the pending stroke preview");
        key(manager,context,'A'); key(manager,context,VK_SPACE);
        check(context.text == L"prefix:一二" && context.active_composition, "Space accumulates an uncommitted phrase");
        check(!manager.element, "Candidates hidden until down");
        key(manager,context,VK_LEFT); key(manager,context,VK_LEFT); key(manager,context,VK_DOWN);
        check(manager.element != nullptr && context.caret == 7, "Caret selects first draft character");
        check(manager.element.As(&list) == S_OK, "Continuous candidates support UILess");
        check(list->GetString(1,&value) == S_OK, "Replacement available");
        std::wstring replacement(value); SysFreeString(value); list.Reset();
        key(manager,context,'2');
        check(context.text == L"prefix:" + replacement + L"二", "Replace only current character");
        context.fail_end = true; key(manager,context,VK_RETURN);
        check(context.active_composition != nullptr, "Failed finalization retains composition");
        context.fail_end = false; key(manager,context,VK_RETURN); pump();
        check(!context.active_composition && context.text == L"prefix:" + replacement + L"二", "Retry finalizes without duplication");
        const auto committed = context.text;
        key(manager,context,'A'); key(manager,context,VK_SPACE); key(manager,context,VK_ESCAPE); pump();
        check(context.text == committed && !context.active_composition, "Cancel erases only current draft");
        context.async = true;
        key(manager,context,'A'); key(manager,context,VK_SPACE); key(manager,context,'A'); key(manager,context,VK_SPACE); key(manager,context,VK_RETURN);
        for (int i=0;i<8 && context.queued;++i) { check(context.flush() == S_OK, "Async composition update"); pump(); }
        check(context.text == committed + L"一一" && !context.active_composition, "Queued spaces and Enter retain order");
        context.async = false;
        key(manager,context,'A'); key(manager,context,VK_SPACE);
        const auto before_focus = context.text;
        service->OnSetFocus(FALSE); pump();
        check(!context.active_composition && context.text == before_focus, "Focus loss preserves visible text without extra insertion");
        context.reject_composition = true;
        key(manager,context,'A'); check(context.text == before_focus && !context.active_composition, "Rejected composition never edits document");
        key(manager,context,VK_ESCAPE); context.reject_composition = false;
        key(manager,context,'A'); key(manager,context,VK_SPACE);
        check(context.active_composition != nullptr, "Can resume after host refusal");
        auto externally_ended = context.active_composition;
        check(externally_ended->EndComposition(1) == S_OK, "Host terminates composition"); externally_ended.Reset();
        const auto external_text = context.text;
        key(manager,context,'A'); key(manager,context,VK_SPACE); key(manager,context,VK_RETURN);
        check(context.text == external_text + L"一", "External termination clears internal draft");
        service->Deactivate(); service.Reset(); pump();
        check(live_objects == 0 && !context.active_composition, "Continuous lifetime cleanup");
        // Phrase integration uses an isolated lexicon and dictionary, never the packaged files.
        fixture_module = root / L"phrase-host" / L"stroke_tsf.dll";
        const auto bundle = fixture_module.parent_path()/L"dictionary";
        std::filesystem::create_directories(bundle/L"phrases");
        const auto encoded_dictionary = encode_index({{"1",U'女',1000000},{"1",U'你',900000},{"2",U'子',1000000},{"2",U'好',900000},{"3",U'世',1},{"4",U'界',1},{"5",U'們',1},{"5",U'人',1}},
            {index_format_version,"phrase-fixture","test"},"phrase fixture");
        check(std::holds_alternative<std::string>(encoded_dictionary),"Phrase host dictionary encode");
        { std::ofstream file(bundle/L"dictionary.sidx",std::ios::binary); file << std::get<std::string>(encoded_dictionary); }
        auto phrase_file = encode_phrases({{U"你好",1000}},"phrase-fixture",std::string(40,'a'));
        check(std::holds_alternative<std::string>(phrase_file),"Phrase host lexicon encode");
        { std::ofstream file(bundle/L"phrases"/L"phrases.pidx",std::ios::binary); file << std::get<std::string>(phrase_file); }
        context.text.clear(); context.caret=0; context.async=true;
        service.Attach(new Service(root));
        check(service->ActivateEx(&manager,1,0)==S_OK,"Phrase-aware service activation");
        key(manager,context,'A');
        check(context.flush()==S_OK,"Async raw stroke update"); pump();
        check(context.text==L"一" && context.caret==1 && context.active_composition,
            "Async composition displays strokes instead of the best matching character");
        key(manager,context,VK_SPACE); key(manager,context,'S');
        for (int i=0;i<8 && context.queued;++i) { check(context.flush()==S_OK,"Pending phrase async update"); pump(); }
        check(context.text==L"女丨" && context.caret==2 && context.active_composition,
            "Accepted character and pending strokes coexist before phrase prediction");
        key(manager,context,VK_SPACE);
        for (int i=0;i<8 && context.queued;++i) { check(context.flush()==S_OK,"Phrase async update"); pump(); }
        check(context.text==L"你好" && context.active_composition,"TSF renders predicted phrase in owned composition");
        context.async=false;
        key(manager,context,VK_LEFT); key(manager,context,VK_DOWN);
        check(manager.element && manager.element.As(&list)==S_OK,"Phrase candidates reopen");
        check(list->GetString(0,&value)==S_OK && std::wstring(value)==L"好","Contextual first candidate agrees with draft");
        SysFreeString(value); list.Reset(); key(manager,context,'2');
        check(context.text==L"女子","Manual replacement updates only owned phrase and preserves lock");
        key(manager,context,VK_RETURN); pump();
        check(!context.active_composition && context.text==L"女子","Phrase Enter commits once");
        service->Deactivate(); service.Reset(); pump();
        check(live_objects==0,"Phrase adapter releases all COM references");
        const auto continuous_learning=std::get<LearningState>(sync_learning(root/L"learning.dat"));
        check(continuous_learning.phrases.at(U"女子").count==1 && !continuous_learning.phrases.contains(U"你好"),
              "Continuous learning observes only final successfully committed characters");
        // Non-continuous associations commit each selection immediately, with no composition.
        const auto association_root = root/L"associations";
        auto association_layout = continuous_layout;
        association_layout.continuous_input = false; association_layout.association_input = true;
        check(std::holds_alternative<std::monostate>(save_layout(association_root/L"layout.dat",association_layout)),"Save association option");
        auto association_file = encode_phrases({{U"你好",1000},{U"你好世界",2000},{U"你們",500},{U"好人",9000}},"phrase-fixture",std::string(40,'a'));
        { std::ofstream file(bundle/L"phrases"/L"phrases.pidx",std::ios::binary); file << std::get<std::string>(association_file); }
        context.text.clear(); context.caret=0; context.async=false;
        service.Attach(new Service(association_root));
        check(service->ActivateEx(&manager,1,TF_TMAE_UIELEMENTENABLEDONLY)==S_OK,"Association UILess activation");
        key(manager,context,'A'); key(manager,context,'2'); pump();
        check(context.text==L"你" && !context.active_composition && manager.element,"First committed word opens association without underline");
        check(manager.element.As(&list)==S_OK && list->GetString(0,&value)==S_OK && std::wstring(value)==L"好","Association priority");
        SysFreeString(value);
        check(list->GetDescription(&value)==S_OK && std::wstring(value)==L"錦筆劃輸入法聯想字","Host can identify association UI");
        SysFreeString(value); list.Reset();
        context.fail_insert=true; key(manager,context,'1'); pump();
        check(context.text==L"你" && manager.element,"Failed association insertion preserves next-word candidates");
        context.fail_insert=false; key(manager,context,'1'); pump();
        check(context.text==L"你好" && manager.element,"Retry inserts only once and extends association context");
        key(manager,context,'1'); pump(); key(manager,context,'1'); pump();
        check(context.text==L"你好世界" && !context.active_composition && !manager.element,"Chained selections finish with no composition");
        for (WPARAM passthrough : {WPARAM(VK_SPACE),WPARAM(VK_RETURN),WPARAM(VK_LEFT),WPARAM(VK_RIGHT),WPARAM(VK_BACK)}) {
            key(manager,context,'A'); key(manager,context,'2'); pump();
            BOOL consumed=TRUE;
            check(service->key_down(&context,passthrough,{},&consumed)==S_OK && !consumed,"Editing keys remain host-owned during associations");
            check(!manager.element,"Host editing key closes association");
        }
        key(manager,context,'A'); key(manager,context,'2'); pump();
        key(manager,context,'S');
        check(manager.element.As(&list)==S_OK && list->GetString(0,&value)==S_OK && std::wstring(value)==L"子","Stroke key replaces association with normal query");
        SysFreeString(value); list.Reset(); key(manager,context,VK_ESCAPE); pump();
        key(manager,context,'A'); key(manager,context,'2'); pump();
        service->OnSetFocus(FALSE);
        check(!manager.element,"Focus loss clears association UI and context");
        key(manager,context,'A'); key(manager,context,'2'); pump();
        check(context.sink->OnEndEdit(&context,1,nullptr)==S_OK && !manager.element,"External edit clears association");
        key(manager,context,'A'); key(manager,context,'2'); pump();
        context.async=true; key(manager,context,'1');
        const auto before_wrong_position = context.text;
        context.caret=0;
        check(context.flush()==TF_E_DISCONNECTED && context.text==before_wrong_position,"Async association refuses a moved caret even without host notification"); pump();
        check(!manager.element,"Moved-caret rejection clears stale candidates");
        context.async=false; context.caret=static_cast<LONG>(context.text.size());
        context.fail_caret=true; key(manager,context,'A'); key(manager,context,'2'); pump();
        check(!manager.element,"Successful insertion without a reliable caret does not open association");
        context.fail_caret=false; context.caret=static_cast<LONG>(context.text.size());
        service->Deactivate(); service.Reset(); pump();
        auto association_learning = std::get<LearningState>(sync_learning(association_root/L"learning.dat"));
        check(association_learning.uses.at(U'好').count==1 && association_learning.uses.at(U'世').count==1 && association_learning.uses.at(U'界').count==1,
              "Only successful association inserts learned");
        check(association_learning.phrases.at(U"你好").count==1 && association_learning.phrases.at(U"你好世界").count==1,
              "Failed retry, moved caret, and focus changes cannot create duplicate contextual learning");
        association_layout.reverse_selection=true;
        check(std::holds_alternative<std::monostate>(save_layout(association_root/L"layout.dat",association_layout)),"Reverse associations option");
        check(std::holds_alternative<LearningState>(set_learning_enabled(association_root/L"learning.dat",false)),"Disable learning for associations");
        manager.native_ui = true;
        service.Attach(new Service(association_root));
        check(service->ActivateEx(&manager,1,0)==S_OK,"Association works with learning disabled");
        key(manager,context,'A'); key(manager,context,'8'); pump();
        check(manager.element,"Learning disabled still offers associations");
        BOOL association_shown{};
        check(manager.element->IsShown(&association_shown)==S_OK && association_shown,
              "Native association candidate element is shown after committing a character");
        BOOL visible_candidate{};
        EnumThreadWindows(GetCurrentThreadId(), [](HWND window, LPARAM found) -> BOOL {
            wchar_t name[64]{};
            if (GetClassNameW(window,name,64) && std::wstring_view(name)==L"StrokeIME.Candidates.v1" && IsWindowVisible(window))
                *reinterpret_cast<BOOL*>(found)=TRUE;
            return TRUE;
        }, reinterpret_cast<LPARAM>(&visible_candidate));
        check(visible_candidate,"Native association creates a visible candidate window in this test process");
        key(manager,context,'9'); pump();
        check(context.text.ends_with(L"你好"),"Reverse digit selects association candidate");
        key(manager,context,VK_ESCAPE); pump(); service->Deactivate(); service.Reset();
        manager.native_ui = false;
        check(live_objects==0,"Association lifecycle releases COM references");
        const auto personal_root=root/L"personal";
        check(std::holds_alternative<std::monostate>(save_layout(personal_root/L"layout.dat",association_layout)),"Personal association settings");
        auto personal_state=std::get<LearningState>(add_personal_phrase(personal_root/L"learning.dat",U"你錦筆"));
        service.Attach(new Service(personal_root));
        check(service->ActivateEx(&manager,1,0)==S_OK,"Personal dictionary activation");
        key(manager,context,'A'); key(manager,context,'8'); pump();
        check(manager.element && manager.element.As(&list)==S_OK && list->GetString(0,&value)==S_OK && std::wstring(value)==L"錦",
              "Manual missing word participates in TSF candidate UI");
        SysFreeString(value); list.Reset();
        key(manager,context,'9'); pump(); key(manager,context,'9'); pump();
        check(context.text.ends_with(L"你錦筆") && !context.active_composition,"Manual word chains without a composition");
        service->Deactivate(); service.Reset(); pump();
        personal_state=std::get<LearningState>(sync_learning(personal_root/L"learning.dat"));
        check(personal_state.phrases.at(U"你錦筆").manual && personal_state.phrases.at(U"你錦筆").count==1,
              "Successful manual word usage is learned once");
        personal_state=std::get<LearningState>(sync_learning(personal_root/L"learning.dat",personal_state.generation,{},{{U"你們",{5,100,false}}}));
        service.Attach(new Service(personal_root));
        check(service->ActivateEx(&manager,1,0)==S_OK,"Reopen contextual history");
        key(manager,context,'A'); key(manager,context,'8'); pump();
        check(manager.element && manager.element.As(&list)==S_OK && list->GetString(0,&value)==S_OK && std::wstring(value)==L"們",
              "Persisted contextual learning changes association order after reactivation");
        SysFreeString(value); list.Reset(); key(manager,context,VK_ESCAPE);
        service->Deactivate(); service.Reset(); pump();
        check(live_objects==0,"Personal dictionary releases all COM references");
        const auto moved_root=root/L"moved-learning";
        service.Attach(new Service(moved_root));
        check(service->ActivateEx(&manager,1,0)==S_OK,"Plain input also learns personal words");
        key(manager,context,'A'); key(manager,context,'2'); pump();
        --context.caret; // A host can move its caret without sending OnEndEdit.
        key(manager,context,'S'); key(manager,context,'1'); pump();
        service->Deactivate(); service.Reset(); pump();
        const auto moved_learning=std::get<LearningState>(sync_learning(moved_root/L"learning.dat"));
        check(moved_learning.phrases.empty() && moved_learning.uses.at(U'你').count==1 && moved_learning.uses.at(U'子').count==1,
              "Unreported caret movement breaks automatic word learning while still allowing ordinary input");
        const auto automatic_root=root/L"automatic-learning";
        context.caret=static_cast<LONG>(context.text.size());
        service.Attach(new Service(automatic_root));
        check(service->ActivateEx(&manager,1,0)==S_OK,"Automatic vocabulary uses ordinary input");
        for (int i=0;i<3;++i) {
            key(manager,context,'A'); key(manager,context,'2'); pump();
            key(manager,context,'S'); key(manager,context,'1'); pump();
            BOOL consumed=TRUE;
            check(service->key_down(&context,VK_RETURN,{},&consumed)==S_OK && !consumed,
                  "Ordinary Enter separates automatic learning fragments");
        }
        service->Deactivate(); service.Reset(); pump();
        const auto automatic_learning=std::get<LearningState>(sync_learning(automatic_root/L"learning.dat"));
        check(automatic_learning.phrases.at(U"你子").count==3 && !automatic_learning.phrases.at(U"你子").manual,
              "Three successful ordinary fragments create an automatic vocabulary entry");
        association_layout.reverse_selection=false;
        check(std::holds_alternative<std::monostate>(save_layout(automatic_root/L"layout.dat",association_layout)),
              "Enable associations for learned vocabulary");
        service.Attach(new Service(automatic_root));
        check(service->ActivateEx(&manager,1,0)==S_OK,"Read automatic vocabulary after restart");
        key(manager,context,'A'); key(manager,context,'2'); pump();
        check(manager.element && manager.element.As(&list)==S_OK && list->GetString(0,&value)==S_OK && std::wstring(value)==L"子",
              "Automatically learned missing word participates in TSF association UI");
        SysFreeString(value); list.Reset();
        key(manager,context,'1'); pump();
        check(context.text.ends_with(L"你子"),"Automatically learned association commits successfully");
        service->Deactivate(); service.Reset(); pump();
        // Restore the continuous fixture's caret after the association scenarios.
        context.text=L"女子"; context.caret=2;
        { std::ofstream file(bundle/L"phrases"/L"phrases.pidx",std::ios::binary); file << "corrupt"; }
        service.Attach(new Service(root));
        check(service->ActivateEx(&manager,1,0)==S_OK,"Bad optional phrase file cannot block input");
        key(manager,context,'A'); key(manager,context,VK_SPACE); key(manager,context,'S'); key(manager,context,VK_SPACE);
        check(context.text==L"女子女子","Corrupt lexicon falls back to character priorities");
        key(manager,context,VK_ESCAPE); service->Deactivate(); service.Reset(); pump();
        check(live_objects==0,"Fallback lifetime cleanup");
        std::filesystem::remove_all(root);
        std::cout << "UILess lifecycle, host pages, async fallback, ordered keys, cancellation and learning passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
