#pragma once
#include "runtime.hpp"
#include <ctfutb.h>
#include <ctffunc.h>
#include <olectl.h>
#include <wrl/client.h>
#include <functional>
#include <utility>
#include <cwchar>

namespace stroke::win {
// The shell owns layout/DPI/theme rendering. This is the standard IME mode item,
// separate from the profile's fixed "劃" branding icon.
class InputModeButton final : public ITfLangBarItemButton, public ITfSource {
  public:
    explicit InputModeButton(bool english, std::function<void()> toggle)
        : english_(english), toggle_(std::move(toggle)) { ++live_objects; }
    ~InputModeButton() { --live_objects; }
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
        info->dwStyle = TF_LBI_STYLE_BTN_BUTTON | TF_LBI_STYLE_TEXTCOLORICON |
                        TF_LBI_STYLE_HIDDENSTATUSCONTROL;
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
        *icon = nullptr;
        // Monochrome AND/XOR mask: transparent background, black glyph. Windows
        // converts black to the current theme's text color via TEXTCOLORICON.
        const auto dc = CreateCompatibleDC(nullptr);
        const auto mask = CreateBitmap(16, 32, 1, 1, nullptr);
        const auto font = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
            DEFAULT_PITCH, L"Microsoft JhengHei UI");
        if (dc && mask && font) {
            const auto old_bitmap = SelectObject(dc, mask);
            const auto old_font = SelectObject(dc, font);
            RECT top{0, 0, 16, 16}, bottom{0, 16, 16, 32};
            FillRect(dc, &top, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
            FillRect(dc, &bottom, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
            SetTextColor(dc, RGB(0, 0, 0));
            SetBkMode(dc, TRANSPARENT);
            DrawTextW(dc, english_ ? L"英" : L"中", 1, &top, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(dc, old_font);
            SelectObject(dc, old_bitmap);
            ICONINFO info{};
            info.fIcon = TRUE;
            info.hbmMask = mask;
            *icon = CreateIconIndirect(&info);
        }
        if (font) DeleteObject(font);
        if (mask) DeleteObject(mask);
        if (dc) DeleteDC(dc);
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
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE UnadviseSink(DWORD cookie) override {
        if (!sink_ || cookie != cookie_) return CONNECT_E_NOCONNECTION;
        sink_.Reset();
        return S_OK;
    }
    void english(bool english) {
        if (english_ == english) return;
        english_ = english;
        notify(TF_LBI_ICON | TF_LBI_TEXT | TF_LBI_TOOLTIP);
    }
    void detach() noexcept { toggle_ = {}; sink_.Reset(); }
  private:
    void notify(DWORD flags) {
        const auto sink = sink_;
        if (sink) (void)sink->OnUpdate(flags);
    }
    std::atomic<ULONG> refs_{1};
    bool english_{}, shown_{true};
    std::function<void()> toggle_;
    Microsoft::WRL::ComPtr<ITfLangBarItemSink> sink_;
    DWORD cookie_{};
};
} // namespace stroke::win
