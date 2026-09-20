#pragma once
#include <windows.h>
#include <msctf.h>

namespace stroke::win {
inline constexpr CLSID service_id{0x65eab844,0x8f22,0x46ef,{0xad,0x6c,0x34,0x43,0xc1,0x99,0x82,0x60}};
inline constexpr GUID profile_id{0xed3297bc,0x5b11,0x4b13,{0x91,0xb4,0x15,0xa4,0x40,0xf4,0xe1,0x2e}};
inline constexpr LANGID language_id = 0x0404;
inline constexpr wchar_t service_name[] = L"錦筆劃輸入法";
inline constexpr wchar_t class_key[] = L"Software\\Classes\\CLSID\\{65EAB844-8F22-46EF-AD6C-3443C1998260}";
}
