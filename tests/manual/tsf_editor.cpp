#include "../../src/windows/tsf/identity.hpp"
#include <richedit.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
namespace {
HWND editor{}, status{};
HFONT font{};
LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM w, LPARAM l) {
    switch (message) {
    case WM_CREATE:
        CreateWindowW(L"BUTTON", L"Activate Jin Stroke", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      16, 16, 220, 36, window, reinterpret_cast<HMENU>(1), nullptr, nullptr);
        status = CreateWindowW(L"STATIC", L"Blank RichEdit test. No files are opened or saved.",
                              WS_CHILD | WS_VISIBLE, 16, 60, 760, 30, window, nullptr, nullptr, nullptr);
        editor = CreateWindowExW(WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                 ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL, 16, 100, 760, 260,
                                 window, nullptr, nullptr, nullptr);
        font = CreateFontW(32, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                           DEFAULT_PITCH, L"Microsoft JhengHei UI");
        SendMessageW(editor, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        return 0;
    case WM_COMMAND:
        if (LOWORD(w) == 1) {
            SetFocus(editor);
            ComPtr<ITfInputProcessorProfileMgr> profiles;
            HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                          IID_PPV_ARGS(&profiles));
            if (SUCCEEDED(hr)) hr = profiles->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR,
                 stroke::win::language_id, stroke::win::service_id, stroke::win::profile_id,
                 nullptr, TF_IPPMF_FORPROCESS);
            SetWindowTextW(status, SUCCEEDED(hr) ? L"Jin Stroke active: type strokes, Space, Left/Right, Down, Enter."
                                                 : L"Could not activate Jin Stroke.");
            return 0;
        }
        break;
    case WM_SIZE:
        MoveWindow(editor, 16, 100, LOWORD(l) - 32, HIWORD(l) - 116, TRUE);
        return 0;
    case WM_DESTROY:
        DeleteObject(font);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, w, l);
}
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 1;
    HMODULE rich = LoadLibraryW(L"Msftedit.dll");
    WNDCLASSW cls{}; cls.lpfnWndProc = procedure; cls.hInstance = instance;
    cls.lpszClassName = L"StrokeIME.ManualEditor"; cls.hCursor = LoadCursorW(nullptr, IDC_IBEAM);
    cls.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&cls);
    HWND window = CreateWindowW(cls.lpszClassName, L"Jin Stroke - blank composition test",
                                WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 830, 430,
                                nullptr, nullptr, instance, nullptr);
    ShowWindow(window, show);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message); DispatchMessageW(&message);
    }
    FreeLibrary(rich); CoUninitialize(); return 0;
}
