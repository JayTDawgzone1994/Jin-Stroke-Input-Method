#pragma once
#include <windows.h>
#include <stroke/engine/session.hpp>
#include <string>

namespace stroke::win {
std::wstring utf16(std::u32string_view text);
class CandidateWindow final {
public:
    explicit CandidateWindow(HINSTANCE instance) noexcept : instance_(instance) {}
    ~CandidateWindow();
    CandidateWindow(const CandidateWindow&) = delete;
    CandidateWindow& operator=(const CandidateWindow&) = delete;
    void present(const SessionSnapshot& snapshot, POINT anchor, HWND owner = nullptr, bool reverse_selection = false);
    void hide() noexcept;
private:
    static LRESULT CALLBACK procedure(HWND, UINT, WPARAM, LPARAM) noexcept;
    void paint() noexcept;
    void refresh_theme() noexcept;
    void round_corners() noexcept;
    HINSTANCE instance_{};
    HWND window_{};
    HFONT font_{};
    std::wstring text_;
    int height_{340};
    COLORREF background_{RGB(250,250,250)};
    COLORREF foreground_{RGB(24,24,24)};
    COLORREF border_{RGB(210,210,210)};
};
}
