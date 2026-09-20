#include "runtime.hpp"
#include "resources/resource.h"
#include <wrl/client.h>
#include <string>

namespace stroke::win {
using Microsoft::WRL::ComPtr;
namespace {
struct ComScope {
    HRESULT hr{CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)};
    ~ComScope() { if (SUCCEEDED(hr)) CoUninitialize(); }
    bool valid() const { return SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE; }
};
HRESULT set_string(HKEY root, const std::wstring& path, const wchar_t* name, const std::wstring& value) {
    HKEY key{};
    LSTATUS error = RegCreateKeyExW(root, path.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr);
    if (error != ERROR_SUCCESS) return HRESULT_FROM_WIN32(error);
    error = RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
        static_cast<DWORD>((value.size()+1)*sizeof(wchar_t)));
    RegCloseKey(key); return HRESULT_FROM_WIN32(error);
}
}
HRESULT register_server() noexcept {
    try {
        ComScope com; if (!com.valid()) return com.hr;
        const auto path = module_path().wstring();
        // Refuse an incomplete deployment before changing machine registration.
        if (!std::filesystem::is_regular_file(module_path().parent_path()/L"dictionary"/L"dictionary.sidx"))
            return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
        if (!FindResourceW(module, MAKEINTRESOURCEW(IDI_STROKE), RT_GROUP_ICON))
            return HRESULT_FROM_WIN32(ERROR_RESOURCE_NAME_NOT_FOUND);
        HRESULT hr = set_string(HKEY_LOCAL_MACHINE,class_key,nullptr,service_name);
        if (FAILED(hr)) return hr;
        const std::wstring inproc = std::wstring(class_key)+L"\\InprocServer32";
        hr = set_string(HKEY_LOCAL_MACHINE,inproc,nullptr,path);
        if (SUCCEEDED(hr)) hr = set_string(HKEY_LOCAL_MACHINE,inproc,L"ThreadingModel",L"Apartment");
        if (FAILED(hr)) return hr;
        ComPtr<ITfInputProcessorProfileMgr> profiles;
        hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&profiles));
        if (FAILED(hr)) return hr;
        hr = profiles->RegisterProfile(service_id,language_id,profile_id,service_name,
            static_cast<ULONG>(std::size(service_name)-1),path.c_str(),static_cast<ULONG>(path.size()),
            STROKE_PROFILE_ICON_INDEX,nullptr,0,TRUE,0);
        if (FAILED(hr)) return hr;
        ComPtr<ITfCategoryMgr> categories;
        hr = CoCreateInstance(CLSID_TF_CategoryMgr,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&categories));
        if (SUCCEEDED(hr)) hr = categories->RegisterCategory(service_id,GUID_TFCAT_TIP_KEYBOARD,service_id);
        if (SUCCEEDED(hr)) hr = categories->RegisterCategory(service_id,GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT,service_id);
        if (SUCCEEDED(hr)) hr = categories->RegisterCategory(service_id,GUID_TFCAT_TIPCAP_UIELEMENTENABLED,service_id);
        // The shell otherwise falls back to the language abbreviation (繁體),
        // even when RegisterProfile supplies a valid embedded branding icon.
        if (SUCCEEDED(hr)) hr = categories->RegisterCategory(service_id,GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT,service_id);
        return hr;
    } catch (...) { return E_FAIL; }
}
HRESULT unregister_server() noexcept {
    ComScope com; if (!com.valid()) return com.hr;
    HRESULT first = S_OK;
    ComPtr<ITfCategoryMgr> categories;
    HRESULT hr = CoCreateInstance(CLSID_TF_CategoryMgr,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&categories));
    if (SUCCEEDED(hr)) hr = categories->UnregisterCategory(service_id,GUID_TFCAT_TIP_KEYBOARD,service_id);
    if (FAILED(hr)) first = hr;
    if (categories) {
        hr = categories->UnregisterCategory(service_id,GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT,service_id);
        if (FAILED(hr) && SUCCEEDED(first)) first = hr;
        hr = categories->UnregisterCategory(service_id,GUID_TFCAT_TIPCAP_UIELEMENTENABLED,service_id);
        if (FAILED(hr) && SUCCEEDED(first)) first = hr;
        hr = categories->UnregisterCategory(service_id,GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT,service_id);
        if (FAILED(hr) && SUCCEEDED(first)) first = hr;
    }
    ComPtr<ITfInputProcessorProfiles> profiles;
    hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&profiles));
    if (SUCCEEDED(hr)) hr = profiles->Unregister(service_id);
    if (FAILED(hr) && SUCCEEDED(first)) first = hr;
    // TSF profile/category data is shared across architectures on modern Windows.
    // The second unregistration may report failure because the first removed it.
    // Accept that only when the complete service registration is confirmed absent;
    // access-denied and other registry errors must still be reported.
    if (FAILED(first)) {
        HKEY remaining{};
        const auto check = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\Microsoft\\CTF\\TIP\\{65EAB844-8F22-46EF-AD6C-3443C1998260}",
            0, KEY_READ, &remaining);
        if (check == ERROR_FILE_NOT_FOUND || check == ERROR_PATH_NOT_FOUND) first = S_OK;
        if (check == ERROR_SUCCESS) RegCloseKey(remaining);
    }
    const auto error = RegDeleteTreeW(HKEY_LOCAL_MACHINE,class_key);
    if (error != ERROR_SUCCESS && error != ERROR_FILE_NOT_FOUND && SUCCEEDED(first)) first = HRESULT_FROM_WIN32(error);
    return first;
}
}
