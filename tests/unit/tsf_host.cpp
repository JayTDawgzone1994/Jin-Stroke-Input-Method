// Include the concrete adapter to exercise the same implementation with isolated storage.
#include "../../src/windows/tsf/service.cpp"
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
struct Context final : ITfContext, ITfInsertAtSelection, ITfSource {
    REFS;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (!out) return E_POINTER; *out = nullptr;
        if (iid == IID_IUnknown || iid == IID_ITfContext) *out = static_cast<ITfContext*>(this);
        else if (iid == IID_ITfInsertAtSelection) *out = static_cast<ITfInsertAtSelection*>(this);
        else if (iid == IID_ITfSource) *out = static_cast<ITfSource*>(this);
        else return E_NOINTERFACE;
        AddRef(); return S_OK;
    }
    bool async{}, refuse{}, fail_insert{}, readonly{};
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
    HRESULT STDMETHODCALLTYPE InsertTextAtSelection(TfEditCookie, DWORD, const WCHAR* value, LONG count, ITfRange** range) override {
        *range = nullptr;
        if (fail_insert) return E_FAIL;
        ++insertions; text.append(value, static_cast<std::size_t>(count)); return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetStatus(TF_STATUS* out) override { *out = {}; out->dwDynamicFlags = readonly ? TS_SD_READONLY : 0; return S_OK; }
    HRESULT STDMETHODCALLTYPE GetDocumentMgr(ITfDocumentMgr** out) override { *out = document; if (*out) (*out)->AddRef(); return S_OK; }
    HRESULT STDMETHODCALLTYPE AdviseSink(REFIID iid, IUnknown* value, DWORD* cookie) override {
        if (iid != IID_ITfTextEditSink) return E_NOINTERFACE;
        *cookie = 1; return value->QueryInterface(IID_PPV_ARGS(&sink));
    }
    HRESULT STDMETHODCALLTYPE UnadviseSink(DWORD) override { sink.Reset(); return S_OK; }
    STUB(InWriteSession, (TfClientId, BOOL*))
    STUB(GetSelection, (TfEditCookie, ULONG, ULONG, TF_SELECTION*, ULONG*))
    STUB(SetSelection, (TfEditCookie, ULONG, const TF_SELECTION*))
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
    bool repeat_show{};
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
        ++begins; element = value; *show = FALSE; *id = 7;
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
        fixture_module = argv[1];
        const auto root = std::filesystem::current_path() / (L"tsf-host-" + std::to_wstring(GetCurrentProcessId()));
        std::filesystem::create_directories(root);
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
        std::filesystem::remove_all(root);
        std::cout << "UILess lifecycle, host pages, async fallback, ordered keys, cancellation and learning passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
