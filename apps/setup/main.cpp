#include "identity.hpp"
#include "resources/resource.h"
#include <windows.h>
#include <msctf.h>
#include <wrl/client.h>
#include <filesystem>
#include <iostream>
#include <string>

using Microsoft::WRL::ComPtr;
using namespace stroke::win;
namespace {
using ServerCall = HRESULT(WINAPI*)();
using GetFactory = HRESULT(WINAPI*)(REFCLSID,REFIID,void**);
using InstallTip = BOOL(WINAPI*)(LPCWSTR,DWORD);
// memcpy avoids MSVC's unsafe FARPROC cast diagnostic for documented dynamic exports.
template<class T> T symbol(HMODULE module, const char* name) {
    const auto address = GetProcAddress(module,name); T result{};
    static_assert(sizeof(result)==sizeof(address)); memcpy(&result,&address,sizeof(result)); return result;
}
struct Library {
    HMODULE value{};
    ~Library() { if (value) FreeLibrary(value); }
};
HRESULT enable(bool enabled) {
    Library lib{LoadLibraryExW(L"input.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32)};
    if (!lib.value) return HRESULT_FROM_WIN32(GetLastError());
    const auto call = symbol<InstallTip>(lib.value,"InstallLayoutOrTip");
    if (!call) return E_NOINTERFACE;
    constexpr wchar_t tip[] = L"0x0404:{65EAB844-8F22-46EF-AD6C-3443C1998260}{ED3297BC-5B11-4B13-91B4-15A440F4E12E}";
    return call(tip,enabled ? 0 : 1) ? S_OK : E_FAIL;
}
HRESULT selftest(HMODULE dll) {
    // Verify the embedded multi-size icon can be loaded from the actual deployment DLL.
    for (const int size : {16,20,24,32,40,48,64,256}) {
        const auto icon = static_cast<HICON>(LoadImageW(dll, MAKEINTRESOURCEW(IDI_STROKE), IMAGE_ICON, size, size, 0));
        if (!icon) return HRESULT_FROM_WIN32(GetLastError());
        DestroyIcon(icon);
    }
    const auto get = symbol<GetFactory>(dll,"DllGetClassObject");
    const auto unload = symbol<ServerCall>(dll,"DllCanUnloadNow");
    if (!get || !unload || unload()!=S_OK) return E_FAIL;
    {
        ComPtr<IClassFactory> factory;
        HRESULT hr = get(service_id,IID_PPV_ARGS(&factory));
        if (FAILED(hr) || unload()!=S_FALSE) return E_FAIL;
        ComPtr<ITfTextInputProcessorEx> service;
        hr = factory->CreateInstance(nullptr,IID_PPV_ARGS(&service));
        if (FAILED(hr)) return hr;
        ComPtr<ITfKeyEventSink> keys;
        if (FAILED(service.As(&keys))) return E_FAIL;
        BOOL eaten=TRUE;
        if (keys->OnTestKeyDown(nullptr,'W',0,&eaten)!=S_OK || eaten) return E_FAIL;
        if (service->ActivateEx(nullptr,TF_CLIENTID_NULL,0)!=E_INVALIDARG) return E_FAIL;
        if (FAILED(service->Deactivate())) return E_FAIL;
    }
    return unload()==S_OK ? S_OK : E_FAIL;
}
HRESULT verify_registered() {
    ComPtr<ITfThreadMgr> manager;
    HRESULT hr=CoCreateInstance(CLSID_TF_ThreadMgr,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&manager));
    if (FAILED(hr)) return hr;
    TfClientId client{};
    hr=manager->Activate(&client);
    if (FAILED(hr)) return hr;
    ComPtr<ITfDocumentMgr> document;
    ComPtr<ITfContext> context;
    TfEditCookie cookie{};
    hr=manager->CreateDocumentMgr(&document);
    if (SUCCEEDED(hr)) hr=document->CreateContext(client,0,nullptr,&context,&cookie);
    if (SUCCEEDED(hr)) hr=document->Push(context.Get());
    if (SUCCEEDED(hr)) hr=manager->SetFocus(document.Get());
    if (FAILED(hr)) { manager->Deactivate(); return hr; }
    ComPtr<ITfInputProcessorProfileMgr> profiles;
    hr=CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&profiles));
    if (SUCCEEDED(hr)) hr=profiles->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR,language_id,
        service_id,profile_id,nullptr,TF_IPPMF_FORPROCESS | TF_IPPMF_DONTCARECURRENTINPUTLANGUAGE);
    std::wcout << L"ActivateProfile: 0x" << std::hex << static_cast<unsigned long>(hr) << L'\n';
    if (SUCCEEDED(hr)) {
        // Activation may be delivered through the STA message queue.
        const auto deadline=GetTickCount64()+250;
        while (GetTickCount64()<deadline) {
            MSG message{};
            while (PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {
                TranslateMessage(&message); DispatchMessageW(&message);
            }
            MsgWaitForMultipleObjectsEx(0,nullptr,10,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
        }
        wchar_t loaded_path[32768]{};
        const auto loaded=GetModuleHandleW(L"stroke_tsf.dll");
        if (loaded && GetModuleFileNameW(loaded,loaded_path,static_cast<DWORD>(std::size(loaded_path))))
            // Keep diagnostic output ASCII-safe even with the default C stream locale.
            std::wcout << L"Service DLL loaded\n";
        else std::wcout << L"Service DLL not loaded\n";
        ComPtr<ITfKeystrokeMgr> keys;
        hr=manager.As(&keys);
        CLSID foreground{};
        if (SUCCEEDED(hr)) hr=keys->GetForeground(&foreground);
        if (SUCCEEDED(hr) && foreground!=service_id) hr=E_FAIL;
        std::wcout << L"Foreground key sink: 0x" << std::hex << static_cast<unsigned long>(hr) << L'\n';
        if (FAILED(hr)) std::wcout << L"Select StrokeIME in the Windows input switcher, then retry this check.\n";
    }
    manager->SetFocus(nullptr);
    document->Pop(TF_POPF_ALL);
    manager->Deactivate();
    return hr;
}
}
int wmain(int argc, wchar_t** argv) {
    if (argc < 2) {
        std::wcout << L"stroke_setup selftest|register|unregister DLL\n"
                      L"stroke_setup enable|disable|status|activate|verify-registered\n";
        return 2;
    }
    HRESULT init = CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if (FAILED(init)) return 1;
    HRESULT hr = E_INVALIDARG;
    try {
        const std::wstring command=argv[1];
        if (command==L"verify-registered") hr=verify_registered();
        else if (command==L"enable" || command==L"disable") hr=enable(command==L"enable");
        else if (command==L"status" || command==L"activate") {
            ComPtr<ITfInputProcessorProfileMgr> profiles;
            hr=CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&profiles));
            if (SUCCEEDED(hr)) {
                if (command==L"activate") hr=profiles->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR,language_id,
                    service_id,profile_id,nullptr,TF_IPPMF_FORSESSION);
                else {
                    TF_INPUTPROCESSORPROFILE profile{};
                    hr=profiles->GetProfile(TF_PROFILETYPE_INPUTPROCESSOR,language_id,service_id,profile_id,nullptr,&profile);
                    if (SUCCEEDED(hr)) std::wcout << L"Registered profile. Flags: 0x" << std::hex << profile.dwFlags << L'\n';
                    TF_INPUTPROCESSORPROFILE active{};
                    if (SUCCEEDED(profiles->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD,&active)))
                        std::wcout << L"Current profile is StrokeIME: " << (active.clsid==service_id ? L"yes" : L"no") << L'\n';
                }
            }
        } else if (argc==3 && (command==L"selftest" || command==L"register" || command==L"unregister")) {
            const auto path=std::filesystem::absolute(argv[2]);
            Library dll{LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32)};
            if (!dll.value) hr=HRESULT_FROM_WIN32(GetLastError());
            else if (command==L"selftest") hr=selftest(dll.value);
            else {
                const auto call=symbol<ServerCall>(dll.value,command==L"register" ? "DllRegisterServer" : "DllUnregisterServer");
                hr=call ? call() : E_NOINTERFACE;
            }
        }
    } catch (...) { hr=E_FAIL; }
    CoUninitialize();
    std::wcout << L"HRESULT 0x" << std::hex << static_cast<unsigned long>(hr) << L'\n';
    return FAILED(hr) ? 1 : 0;
}
