#include "../../src/windows/tsf/input_mode_button.hpp"
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <string_view>

namespace stroke::win {
std::atomic<long> live_objects{};
}
using namespace stroke::win;
using Microsoft::WRL::ComPtr;
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void check_rendered_icon(bool english, COLORREF foreground) {
    HICON icon = input_mode_icon(english, foreground);
    check(icon != nullptr, "Create explicitly colored mode icon");
    ICONINFO info{};
    check(GetIconInfo(icon, &info) && info.hbmColor, "Mode icon has a color bitmap");
    DeleteObject(info.hbmColor);
    DeleteObject(info.hbmMask);
    // Exercise shell-sized rendering on an opposite background without changing
    // the user's theme, settings, focus or registered input method.
    for (int size : {16, 24, 32, 48}) {
        const auto dc = CreateCompatibleDC(nullptr);
        BITMAPINFO bitmap{};
        bitmap.bmiHeader = {sizeof(BITMAPINFOHEADER), size, -size, 1, 32, BI_RGB};
        void* pixels{};
        const auto image = CreateDIBSection(dc, &bitmap, DIB_RGB_COLORS, &pixels, nullptr, 0);
        check(dc && image && pixels, "Create icon rendering surface");
        const auto old = SelectObject(dc, image);
        const auto background = foreground == RGB(0, 0, 0) ? 0xffffffu : 0u;
        auto* data = static_cast<std::uint32_t*>(pixels);
        std::fill_n(data, size * size, background);
        check(DrawIconEx(dc, 0, 0, icon, size, size, 0, nullptr, DI_NORMAL), "Render alpha icon");
        GdiFlush();
        int glyph_pixels{}, background_pixels{};
        const auto expected = static_cast<std::uint32_t>((GetRValue(foreground) << 16) |
                              (GetGValue(foreground) << 8) | GetBValue(foreground));
        for (int i = 0; i < size * size; ++i) {
            const auto rgb = data[i] & 0xffffffu;
            if (rgb == expected) ++glyph_pixels;
            if (rgb == background) ++background_pixels;
        }
        check(glyph_pixels > 0 && background_pixels > 0,
              "Glyph keeps the requested color and background stays transparent at every scale");
        SelectObject(dc, old);
        DeleteObject(image);
        DeleteDC(dc);
    }
    DestroyIcon(icon);
}
BOOL CALLBACK find_theme_window(HWND window, LPARAM result) {
    wchar_t name[80]{};
    GetClassNameW(window, name, 80);
    if (std::wstring_view(name) != L"StrokeIME.InputModeTheme.v1") return TRUE;
    *reinterpret_cast<HWND*>(result) = window;
    return FALSE;
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
                  info.clsidService == service_id && !(info.dwStyle & TF_LBI_STYLE_TEXTCOLORICON),
              "Mode icon uses the standard tray item with explicit colors");
        check(input_mode_color(false, false, 0) == RGB(255, 255, 255) &&
                  input_mode_color(true, false, 0) == RGB(0, 0, 0),
              "Dark Windows mode uses white and light Windows mode uses black");
        const auto contrast_text = RGB(255, 255, 0);
        check(input_mode_color(true, true, contrast_text) == contrast_text &&
                  input_mode_color(false, true, contrast_text) == contrast_text,
              "High contrast overrides both Windows color modes");
        for (bool english : {false, true})
            for (auto color : {RGB(255, 255, 255), RGB(0, 0, 0), contrast_text})
                check_rendered_icon(english, color);
        BSTR text{};
        check(button->GetText(&text) == S_OK && std::wstring(text) == L"中", "Chinese mode label");
        SysFreeString(text);
        ModeSink sink;
        DWORD cookie{};
        check(button->AdviseSink(IID_ITfLangBarItemSink, &sink, &cookie) == S_OK,
              "Subscribe mode icon changes");
        HWND theme_window{};
        EnumThreadWindows(GetCurrentThreadId(), find_theme_window, reinterpret_cast<LPARAM>(&theme_window));
        check(theme_window && !IsWindowVisible(theme_window), "Theme observer stays hidden");
        for (auto message : {WM_SETTINGCHANGE, WM_THEMECHANGED, WM_SYSCOLORCHANGE}) {
            sink.changed = 0;
            SendMessageW(theme_window, message, 0, 0);
            check(sink.changed == TF_LBI_ICON, "System theme changes invalidate the shell icon");
        }
        sink.changed = 0;
        button->english(true);
        check((sink.changed & TF_LBI_ICON) && (sink.changed & TF_LBI_TEXT), "Notify the shell on mode changes");
        check(button->GetText(&text) == S_OK && std::wstring(text) == L"英", "English mode label");
        SysFreeString(text);
        HICON icon{};
        check(button->GetIcon(&icon) == S_OK && icon, "Provide a system-colored mode icon");
        DestroyIcon(icon);
        check(button->OnClick(TF_LBI_CLK_LEFT, {}, nullptr) == S_OK && clicks == 1,
              "Mode icon and Shift can use the same switch operation");
        check(button->UnadviseSink(cookie + 1) == CONNECT_E_NOCONNECTION,
              "Reject stale shell subscriptions");
        button->detach();
        check(!IsWindow(theme_window), "Deactivate the theme observer with the service");
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
