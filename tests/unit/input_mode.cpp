#include "../../src/windows/tsf/input_mode_button.hpp"
#include <iostream>
#include <stdexcept>

namespace stroke::win {
std::atomic<long> live_objects{};
}
using namespace stroke::win;
using Microsoft::WRL::ComPtr;
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
struct ModeSink final : ITfLangBarItemSink {
    ULONG refs{1};
    DWORD changed{};
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (iid != IID_IUnknown && iid != IID_ITfLangBarItemSink) return E_NOINTERFACE;
        *out = static_cast<ITfLangBarItemSink*>(this); AddRef(); return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override { return --refs; }
    HRESULT STDMETHODCALLTYPE OnUpdate(DWORD flags) override { changed |= flags; return S_OK; }
};
int main() {
    try {
        int clicks{};
        ComPtr<InputModeButton> button;
        button.Attach(new InputModeButton(false, [&] { ++clicks; }));
        TF_LANGBARITEMINFO info{};
        check(button->GetInfo(&info) == S_OK && info.guidItem == GUID_LBI_INPUTMODE &&
                  info.clsidService == service_id && (info.dwStyle & TF_LBI_STYLE_TEXTCOLORICON),
              "Mode icon uses the standard tray item and theme recoloring");
        BSTR text{};
        check(button->GetText(&text) == S_OK && std::wstring(text) == L"中", "Chinese mode label");
        SysFreeString(text);
        ModeSink sink;
        DWORD cookie{};
        check(button->AdviseSink(IID_ITfLangBarItemSink, &sink, &cookie) == S_OK,
              "Subscribe mode icon changes");
        button->english(true);
        check((sink.changed & TF_LBI_ICON) && (sink.changed & TF_LBI_TEXT), "Notify the shell on mode changes");
        check(button->GetText(&text) == S_OK && std::wstring(text) == L"英", "English mode label");
        SysFreeString(text);
        HICON icon{};
        check(button->GetIcon(&icon) == S_OK && icon, "Provide a monochrome mode icon");
        DestroyIcon(icon);
        check(button->OnClick(TF_LBI_CLK_LEFT, {}, nullptr) == S_OK && clicks == 1,
              "Mode icon and Shift can use the same switch operation");
        check(button->UnadviseSink(cookie + 1) == CONNECT_E_NOCONNECTION,
              "Reject stale shell subscriptions");
        button->detach();
        check(sink.refs == 1 && button->OnClick(TF_LBI_CLK_LEFT, {}, nullptr) == TF_E_DISCONNECTED,
              "Retained shell items cannot call a deactivated service");
        button.Reset();
        check(live_objects == 0, "Mode button lifetime cleanup");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
