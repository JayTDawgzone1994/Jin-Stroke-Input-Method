#include "candidate_window.hpp"
#include "../storage/selection.hpp"
#include <algorithm>
#include <stdexcept>

namespace stroke::win {
std::wstring utf16(std::u32string_view text) {
    std::wstring result;
    for (char32_t cp : text) {
        if (!is_unicode_scalar(cp)) throw std::invalid_argument("Invalid Unicode scalar");
        if (cp <= 0xffff) result.push_back(static_cast<wchar_t>(cp));
        else {
            cp -= 0x10000;
            result.push_back(static_cast<wchar_t>(0xd800 + (cp >> 10)));
            result.push_back(static_cast<wchar_t>(0xdc00 + (cp & 0x3ff)));
        }
    }
    return result;
}
CandidateWindow::~CandidateWindow() {
    if (window_) DestroyWindow(window_);
    if (font_) DeleteObject(font_);
    UnregisterClassW(L"StrokeIME.Candidates.v1", instance_);
}
void CandidateWindow::hide() noexcept {
    if (window_ && IsWindowVisible(window_)) {
        NotifyWinEvent(EVENT_OBJECT_IME_HIDE, window_, OBJID_CLIENT, CHILDID_SELF);
        ShowWindow(window_, SW_HIDE);
    }
}
void CandidateWindow::present(const SessionSnapshot& state, POINT anchor, HWND owner, bool reverse_selection) {
    if (state.phase == SessionPhase::idle) { hide(); return; }
    text_ = L"錦筆劃  ";
    constexpr wchar_t marks[] = L"一丨丿丶フ＊";
    const auto offset = state.strokes.size() > 24 ? state.strokes.size() - 24 : 0;
    if (offset) text_ += L"…";
    for (auto i = offset; i < state.strokes.size(); ++i)
        text_ += marks[static_cast<unsigned>(state.strokes[i]) - 1];
    text_ += L"\n";
    for (std::size_t i = 0; i < state.visible_candidates.size(); ++i) {
        text_ += static_cast<wchar_t>(candidate_digit(i, reverse_selection));
        text_ += L".  ";
        const char32_t cp = state.visible_candidates[i].character;
        text_ += utf16(std::u32string_view(&cp, 1));
        text_ += state.visible_candidates[i].exact_match ? L"  ✓\n" : L"\n";
    }
    if (state.visible_candidates.empty()) text_ += L"無候選字\n";
    text_ += L"第 " + std::to_wstring(state.page_index + 1) + L" 頁 / "
        + std::to_wstring(std::max<std::size_t>(1, (state.total_candidates + 8) / 9))
        + L" 頁";
    height_ = 76 + static_cast<int>(std::max<std::size_t>(1, state.visible_candidates.size())) * 28;
    if (!window_) {
        WNDCLASSEXW wc{sizeof(wc)};
        wc.lpfnWndProc = procedure;
        wc.hInstance = instance_;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"StrokeIME.Candidates.v1";
        if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            throw std::runtime_error("Candidate class registration failed");
        window_ = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
            wc.lpszClassName, L"錦筆劃輸入法候選字", WS_POPUP,
            0, 0, 400, height_, owner, nullptr, instance_, this);
        if (!window_) throw std::runtime_error("Candidate window creation failed");
        font_ = CreateFontW(-20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH, L"Microsoft JhengHei UI");
    }
    MONITORINFO monitor{sizeof(monitor)};
    if (GetMonitorInfoW(MonitorFromPoint(anchor, MONITOR_DEFAULTTONEAREST), &monitor)) {
        anchor.x = std::max(monitor.rcWork.left, std::min(anchor.x, monitor.rcWork.right - 400));
        anchor.y = std::max(monitor.rcWork.top, std::min(anchor.y, monitor.rcWork.bottom - height_));
    }
    // Shell/search surfaces require an owned IME window and light-dismiss events.
    SetWindowLongPtrW(window_, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(owner));
    const bool wasVisible = IsWindowVisible(window_) != FALSE;
    if (!wasVisible) refresh_theme();
    SetWindowPos(window_, HWND_TOPMOST, anchor.x, anchor.y, 400, height_, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    NotifyWinEvent(wasVisible ? EVENT_OBJECT_IME_CHANGE : EVENT_OBJECT_IME_SHOW, window_, OBJID_CLIENT, CHILDID_SELF);
    InvalidateRect(window_, nullptr, TRUE);
}
void CandidateWindow::refresh_theme() noexcept {
    HIGHCONTRASTW contrast{sizeof(contrast)};
    SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0);
    DWORD light = 1, size = sizeof(light);
    const bool dark = RegGetValueW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &size) == ERROR_SUCCESS && light == 0;
    if (contrast.dwFlags & HCF_HIGHCONTRASTON) {
        background_ = GetSysColor(COLOR_WINDOW);
        foreground_ = border_ = GetSysColor(COLOR_WINDOWTEXT);
    } else {
        background_ = dark ? RGB(32,32,32) : RGB(250,250,250);
        foreground_ = dark ? RGB(245,245,245) : RGB(24,24,24);
        border_ = dark ? RGB(76,76,76) : RGB(210,210,210);
    }
    if (window_) InvalidateRect(window_, nullptr, FALSE);
}
void CandidateWindow::round_corners() noexcept {
    RECT area{}; GetClientRect(window_, &area);
    HRGN region = CreateRoundRectRgn(0, 0, area.right + 1, area.bottom + 1, 16, 16);
    // SetWindowRgn takes ownership only on success. No layered window or focus change.
    if (region && !SetWindowRgn(window_, region, TRUE)) DeleteObject(region);
}
LRESULT CALLBACK CandidateWindow::procedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) noexcept {
    auto* self = reinterpret_cast<CandidateWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        self = static_cast<CandidateWindow*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->window_ = hwnd;
    }
    if (msg == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if (msg == WM_ERASEBKGND) return 1;
    if (msg == WM_SIZE && self) { self->round_corners(); return 0; }
    if (self && (msg == WM_SETTINGCHANGE || msg == WM_THEMECHANGED || msg == WM_SYSCOLORCHANGE))
        self->refresh_theme();
    if (msg == WM_PAINT && self) { self->paint(); return 0; }
    if (msg == WM_NCDESTROY && self) {
        self->window_ = nullptr;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
void CandidateWindow::paint() noexcept {
    PAINTSTRUCT ps{};
    HDC dc = BeginPaint(window_, &ps);
    if (!dc) return;
    RECT area{}; GetClientRect(window_, &area);
    HBRUSH brush = CreateSolidBrush(background_);
    HPEN pen = CreatePen(PS_SOLID, 1, border_);
    if (brush) FillRect(dc, &area, brush);
    if (brush && pen) {
        HGDIOBJ oldBrush = SelectObject(dc, brush), oldPen = SelectObject(dc, pen);
        RoundRect(dc, 0, 0, area.right, area.bottom, 16, 16);
        SelectObject(dc, oldPen); SelectObject(dc, oldBrush);
    }
    if (pen) DeleteObject(pen);
    if (brush) DeleteObject(brush);
    SetTextColor(dc, foreground_);
    SetBkMode(dc, TRANSPARENT);
    HGDIOBJ old = font_ ? SelectObject(dc, font_) : nullptr;
    area.left += 16; area.top += 12; area.right -= 12;
    DrawTextW(dc, text_.c_str(), static_cast<int>(text_.size()), &area, DT_LEFT | DT_NOPREFIX);
    if (old) SelectObject(dc, old);
    EndPaint(window_, &ps);
}
}
