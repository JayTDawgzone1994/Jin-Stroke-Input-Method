#pragma once
#include "runtime.hpp"
#include <new>
namespace stroke::win {
inline constexpr GUID composition_attribute_id{
    0xd1671166, 0x0abe, 0x4d0f, {0xa7, 0x64, 0x18, 0xd2, 0x24, 0x9d, 0x13, 0x1d}};
class CompositionAttribute final : public ITfDisplayAttributeInfo {
    std::atomic<ULONG> refs_{1};
    TF_DISPLAYATTRIBUTE value_{};

  public:
    CompositionAttribute() {
        ++live_objects;
        Reset();
    }
    ~CompositionAttribute() { --live_objects; }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (!out)
            return E_POINTER;
        *out = nullptr;
        if (iid != IID_IUnknown && iid != IID_ITfDisplayAttributeInfo)
            return E_NOINTERFACE;
        *out = static_cast<ITfDisplayAttributeInfo*>(this);
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override {
        auto refs = --refs_;
        if (!refs)
            delete this;
        return refs;
    }
    HRESULT STDMETHODCALLTYPE GetGUID(GUID* out) override {
        if (!out)
            return E_POINTER;
        *out = composition_attribute_id;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetDescription(BSTR* out) override {
        if (!out)
            return E_POINTER;
        *out = SysAllocString(L"錦筆劃連續組字");
        return *out ? S_OK : E_OUTOFMEMORY;
    }
    HRESULT STDMETHODCALLTYPE GetAttributeInfo(TF_DISPLAYATTRIBUTE* out) override {
        if (!out)
            return E_POINTER;
        *out = value_;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetAttributeInfo(const TF_DISPLAYATTRIBUTE* value) override {
        if (!value)
            return E_POINTER;
        value_ = *value;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Reset() override {
        value_ = {};
        value_.lsStyle = TF_LS_SOLID;
        value_.bAttr = TF_ATTR_INPUT;
        value_.crLine.type = TF_CT_SYSCOLOR;
        value_.crLine.nIndex = COLOR_WINDOWTEXT;
        return S_OK;
    }
};
class CompositionAttributes final : public IEnumTfDisplayAttributeInfo {
    std::atomic<ULONG> refs_{1};
    bool consumed_{};

  public:
    explicit CompositionAttributes(bool consumed = false) : consumed_(consumed) { ++live_objects; }
    ~CompositionAttributes() { --live_objects; }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (!out)
            return E_POINTER;
        *out = nullptr;
        if (iid != IID_IUnknown && iid != IID_IEnumTfDisplayAttributeInfo)
            return E_NOINTERFACE;
        *out = static_cast<IEnumTfDisplayAttributeInfo*>(this);
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override {
        auto refs = --refs_;
        if (!refs)
            delete this;
        return refs;
    }
    HRESULT STDMETHODCALLTYPE Clone(IEnumTfDisplayAttributeInfo** out) override {
        if (!out)
            return E_POINTER;
        *out = new (std::nothrow) CompositionAttributes(consumed_);
        return *out ? S_OK : E_OUTOFMEMORY;
    }
    HRESULT STDMETHODCALLTYPE Next(ULONG count, ITfDisplayAttributeInfo** out,
                                   ULONG* fetched) override {
        if (!out || (!fetched && count != 1))
            return E_POINTER;
        if (fetched)
            *fetched = 0;
        if (!count)
            return S_OK;
        *out = nullptr;
        if (consumed_)
            return S_FALSE;
        *out = new (std::nothrow) CompositionAttribute;
        if (!*out)
            return E_OUTOFMEMORY;
        consumed_ = true;
        if (fetched)
            *fetched = 1;
        return count == 1 ? S_OK : S_FALSE;
    }
    HRESULT STDMETHODCALLTYPE Reset() override {
        consumed_ = false;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Skip(ULONG count) override {
        if (!count)
            return S_OK;
        const bool available = !consumed_;
        consumed_ = true;
        return available && count == 1 ? S_OK : S_FALSE;
    }
};
} // namespace stroke::win
