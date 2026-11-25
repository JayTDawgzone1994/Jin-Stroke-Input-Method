#pragma once
#include "runtime.hpp"
#include <ctfutb.h>
#include <ctffunc.h>
#include <olectl.h>
#include <wrl/client.h>
#include <functional>
#include <utility>
#include <cwchar>
#include <array>
#include <cstdint>

namespace stroke::win {
inline COLORREF input_mode_color(bool light, bool high_contrast, COLORREF contrast_text) noexcept {
    return high_contrast ? contrast_text : light ? RGB(0, 0, 0) : RGB(255, 255, 255);
}
inline COLORREF input_mode_system_color() noexcept {
    HIGHCONTRASTW contrast{sizeof(contrast)};
    SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0);
    // The taskbar follows the Windows color mode, which can differ from app mode.
    DWORD light = 1, size = sizeof(light);
    (void)RegGetValueW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"SystemUsesLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &size);
    return input_mode_color(light != 0, (contrast.dwFlags & HCF_HIGHCONTRASTON) != 0,
                            GetSysColor(COLOR_WINDOWTEXT));
}
inline HICON input_mode_icon(bool english, COLORREF foreground) noexcept {
    // Use explicit premultiplied alpha: shell theme recoloring is not reliable
    // for GUID_LBI_INPUTMODE on all Windows 11 taskbars.
    const auto dc = CreateCompatibleDC(nullptr);
    BITMAPINFO bitmap{};
    bitmap.bmiHeader = {sizeof(BITMAPINFOHEADER), 16, -16, 1, 32, BI_RGB};
    void* pixels{};
    const auto color = CreateDIBSection(dc, &bitmap, DIB_RGB_COLORS, &pixels, nullptr, 0);
    const std::array<BYTE, 32> transparent_mask{};
    const auto mask = CreateBitmap(16, 16, 1, 1, transparent_mask.data());
    const auto font = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        DEFAULT_PITCH, L"Microsoft JhengHei UI");
    HICON icon{};
    if (dc && color && mask && font && pixels) {
        const auto old_bitmap = SelectObject(dc, color);
        const auto old_font = SelectObject(dc, font);
        RECT area{0, 0, 16, 16};
        FillRect(dc, &area, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        SetTextColor(dc, RGB(255, 255, 255));
        SetBkMode(dc, TRANSPARENT);
        DrawTextW(dc, english ? L"英" : L"中", 1, &area, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        GdiFlush();
        auto* data = static_cast<std::uint32_t*>(pixels);
        for (int i = 0; i < 16 * 16; ++i) {
            const auto alpha = data[i] & 0xff;
            data[i] = (alpha << 24) | ((GetRValue(foreground) * alpha / 255) << 16) |
                      ((GetGValue(foreground) * alpha / 255) << 8) |
                      (GetBValue(foreground) * alpha / 255);
        }
        SelectObject(dc, old_font);
        SelectObject(dc, old_bitmap);
        ICONINFO info{};
        info.fIcon = TRUE;
        info.hbmColor = color;
        info.hbmMask = mask;
        icon = CreateIconIndirect(&info);
    }
    if (font) DeleteObject(font);
    if (mask) DeleteObject(mask);
    if (color) DeleteObject(color);
    if (dc) DeleteDC(dc);
    return icon;
}
// The shell owns layout/DPI rendering. This is the standard IME mode item,
// separate from the profile's fixed "劃" branding icon.
class InputModeButton final : public ITfLangBarItemButton, public ITfSource {
  public:
    explicit InputModeButton(bool english, std::function<void()> toggle)
        : english_(english), toggle_(std::move(toggle)) { ++live_objects; }
    ~InputModeButton() { stop_theme_watch(); --live_objects; }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (iid == IID_IUnknown || iid == IID_ITfLangBarItem || iid == IID_ITfLangBarItemButton)
            *out = static_cast<ITfLangBarItemButton*>(this);
        else if (iid == IID_ITfSource) *out = static_cast<ITfSource*>(this);
        else return E_NOINTERFACE;
        AddRef(); return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override {
        const auto refs = --refs_;
        if (!refs) delete this;
        return refs;
    }
    HRESULT STDMETHODCALLTYPE GetInfo(TF_LANGBARITEMINFO* info) override {
        if (!info) return E_POINTER;
        *info = {};
        info->clsidService = service_id;
        info->guidItem = GUID_LBI_INPUTMODE;
        info->dwStyle = TF_LBI_STYLE_BTN_BUTTON | TF_LBI_STYLE_HIDDENSTATUSCONTROL;
        wcscpy_s(info->szDescription, L"錦筆劃中英文模式");
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetStatus(DWORD* status) override {
        if (!status) return E_POINTER;
        *status = shown_ ? 0 : TF_LBI_STATUS_HIDDEN;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Show(BOOL shown) override {
        const bool visible = shown != FALSE;
        if (shown_ == visible) return S_OK;
        shown_ = visible;
        notify(TF_LBI_STATUS);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetTooltipString(BSTR* text) override {
        if (!text) return E_POINTER;
        *text = SysAllocString(english_ ? L"英文模式（Shift 切換）" : L"筆畫模式（Shift 切換）");
        return *text ? S_OK : E_OUTOFMEMORY;
    }
    HRESULT STDMETHODCALLTYPE GetText(BSTR* text) override {
        if (!text) return E_POINTER;
        *text = SysAllocString(english_ ? L"英" : L"中");
        return *text ? S_OK : E_OUTOFMEMORY;
    }
    HRESULT STDMETHODCALLTYPE GetIcon(HICON* icon) override {
        if (!icon) return E_POINTER;
        *icon = input_mode_icon(english_, input_mode_system_color());
        return *icon ? S_OK : E_FAIL;
    }
    HRESULT STDMETHODCALLTYPE OnClick(TfLBIClick click, POINT, const RECT*) override {
        if (!toggle_) return TF_E_DISCONNECTED;
        if (click != TF_LBI_CLK_LEFT) return S_OK;
        try {
            const auto toggle = toggle_; // Callback can re-enter service teardown.
            toggle();
            return S_OK;
        } catch (...) { return E_FAIL; }
    }
    HRESULT STDMETHODCALLTYPE InitMenu(ITfMenu*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnMenuSelect(UINT) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE AdviseSink(REFIID iid, IUnknown* unknown, DWORD* cookie) override {
        if (!cookie || !unknown) return E_POINTER;
        *cookie = TF_INVALID_COOKIE;
        if (iid != IID_ITfLangBarItemSink) return CONNECT_E_CANNOTCONNECT;
        if (sink_) return CONNECT_E_ADVISELIMIT;
        const auto result = unknown->QueryInterface(IID_PPV_ARGS(&sink_));
        if (FAILED(result)) return result;
        if (++cookie_ == TF_INVALID_COOKIE) ++cookie_;
        *cookie = cookie_;
        start_theme_watch();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE UnadviseSink(DWORD cookie) override {
        if (!sink_ || cookie != cookie_) return CONNECT_E_NOCONNECTION;
        sink_.Reset();
        stop_theme_watch();
        return S_OK;
    }
    void english(bool english) {
        if (english_ == english) return;
        english_ = english;
        notify(TF_LBI_ICON | TF_LBI_TEXT | TF_LBI_TOOLTIP);
    }
    void detach() noexcept { toggle_ = {}; sink_.Reset(); stop_theme_watch(); }
  private:
    static constexpr auto theme_class = L"StrokeIME.InputModeTheme.v1";
    static LRESULT CALLBACK theme_procedure(HWND window, UINT message, WPARAM wp, LPARAM lp) noexcept {
        auto* self = reinterpret_cast<InputModeButton*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            self = static_cast<InputModeButton*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (message == WM_NCDESTROY) SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        if (self && (message == WM_SETTINGCHANGE || message == WM_THEMECHANGED ||
                     message == WM_SYSCOLORCHANGE)) {
            // OnUpdate may re-enter teardown and release the shell's reference.
            self->AddRef();
            try { self->notify(TF_LBI_ICON); } catch (...) {}
            self->Release();
            return 0;
        }
        return DefWindowProcW(window, message, wp, lp);
    }
    void start_theme_watch() noexcept {
        if (theme_window_) return;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&theme_procedure), &theme_module_)) return;
        WNDCLASSEXW wc{sizeof(wc)};
        wc.hInstance = theme_module_;
        wc.lpfnWndProc = theme_procedure;
        wc.lpszClassName = theme_class;
        if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return;
        // A hidden top-level window receives broadcasts; HWND_MESSAGE does not.
        theme_window_ = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, theme_class,
            L"", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, theme_module_, this);
    }
    void stop_theme_watch() noexcept {
        const auto window = std::exchange(theme_window_, nullptr);
        if (window) DestroyWindow(window);
        if (theme_module_) UnregisterClassW(theme_class, theme_module_);
        theme_module_ = nullptr;
    }
    void notify(DWORD flags) {
        const auto sink = sink_;
        if (sink) (void)sink->OnUpdate(flags);
    }
    std::atomic<ULONG> refs_{1};
    bool english_{}, shown_{true};
    std::function<void()> toggle_;
    Microsoft::WRL::ComPtr<ITfLangBarItemSink> sink_;
    DWORD cookie_{};
    HWND theme_window_{};
    HMODULE theme_module_{};
};
} // namespace stroke::win
