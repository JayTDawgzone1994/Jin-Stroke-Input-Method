#include "runtime.hpp"
#include <new>
#include <stdexcept>
#include <string>

namespace stroke::win {
HINSTANCE module{};
std::atomic<long> live_objects{};
std::filesystem::path module_path() {
    std::wstring path(32768, L'\0');
    const DWORD size = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
    if (!size || size >= path.size()) throw std::runtime_error("DLL path unavailable");
    path.resize(size); return path;
}
class Factory final : public IClassFactory {
public:
    Factory() { ++live_objects; }
    ~Factory() { --live_objects; }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (iid != IID_IUnknown && iid != IID_IClassFactory) return E_NOINTERFACE;
        *out = static_cast<IClassFactory*>(this); AddRef(); return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override { ULONG r = --refs_; if (!r) delete this; return r; }
    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* outer, REFIID iid, void** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (outer) return CLASS_E_NOAGGREGATION;
        return create_service(iid,out);
    }
    HRESULT STDMETHODCALLTYPE LockServer(BOOL lock) override {
        if (lock) ++live_objects; else --live_objects;
        return S_OK;
    }
private:
    std::atomic<ULONG> refs_{1};
};
}
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        stroke::win::module = instance;
        // Package builds use the static CRT, which needs thread notifications.
    }
    return TRUE;
}
extern "C" HRESULT __stdcall DllGetClassObject(REFCLSID clsid, REFIID iid, void** out) {
    if (!out) return E_POINTER;
    *out = nullptr;
    if (clsid != stroke::win::service_id) return CLASS_E_CLASSNOTAVAILABLE;
    auto* factory = new (std::nothrow) stroke::win::Factory;
    if (!factory) return E_OUTOFMEMORY;
    const HRESULT hr = factory->QueryInterface(iid,out); factory->Release(); return hr;
}
extern "C" HRESULT __stdcall DllCanUnloadNow() { return stroke::win::live_objects == 0 ? S_OK : S_FALSE; }
extern "C" HRESULT __stdcall DllRegisterServer() { return stroke::win::register_server(); }
extern "C" HRESULT __stdcall DllUnregisterServer() { return stroke::win::unregister_server(); }
