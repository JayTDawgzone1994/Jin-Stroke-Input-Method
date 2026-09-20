#pragma once
#include "runtime.hpp"
#include "../ui/candidate_window.hpp"
#include <wrl/client.h>
#include <functional>
#include <algorithm>

namespace stroke::win {
// A separate COM identity can outlive the composition. detach() removes all service callbacks.
class CandidateElement final : public ITfCandidateListUIElement {
public:
    using Visibility = std::function<void(bool)>;
    using Pagination = std::function<HRESULT(std::vector<std::size_t>)>;
    CandidateElement(ITfDocumentMgr* document, Visibility visibility, Pagination pagination)
        : document_(document), visibility_(std::move(visibility)), pagination_(std::move(pagination)) { ++live_objects; }
    ~CandidateElement() { --live_objects; }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (iid != IID_IUnknown && iid != IID_ITfUIElement && iid != IID_ITfCandidateListUIElement) return E_NOINTERFACE;
        *out = static_cast<ITfCandidateListUIElement*>(this); AddRef(); return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override { const auto r = --refs_; if (!r) delete this; return r; }
    HRESULT STDMETHODCALLTYPE GetDescription(BSTR* out) override {
        if (!out) return E_POINTER;
        *out = SysAllocString(L"錦筆劃輸入法候選字"); return *out ? S_OK : E_OUTOFMEMORY;
    }
    HRESULT STDMETHODCALLTYPE GetGUID(GUID* out) override {
        if (!out) return E_POINTER;
        *out = GUID{0x9e2a8ec1,0x2d8f,0x4a19,{0xb2,0x64,0x57,0xca,0x91,0x9c,0xe5,0x39}}; return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Show(BOOL show) override {
        if (!visibility_) return TF_E_DISCONNECTED;
        try { visibility_(show != FALSE); return S_OK; } catch (...) { return E_FAIL; }
    }
    HRESULT STDMETHODCALLTYPE IsShown(BOOL* out) override {
        if (!out) return E_POINTER;
        *out = shown_; return S_OK;
    }
    void shown(bool value) noexcept { shown_ = value; }
    HRESULT STDMETHODCALLTYPE GetUpdatedFlags(DWORD* out) override {
        if (!out) return E_POINTER;
        *out = TF_CLUIE_DOCUMENTMGR | TF_CLUIE_COUNT | TF_CLUIE_SELECTION | TF_CLUIE_STRING |
            TF_CLUIE_PAGEINDEX | TF_CLUIE_CURRENTPAGE; return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetDocumentMgr(ITfDocumentMgr** out) override {
        if (!out) return E_POINTER;
        return document_.CopyTo(out);
    }
    HRESULT STDMETHODCALLTYPE GetCount(UINT* out) override {
        if (!out) return E_POINTER;
        *out = static_cast<UINT>(strings_.size()); return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetSelection(UINT* out) override {
        if (!out) return E_POINTER;
        *out = pages_.empty() ? 0 : pages_[page_]; return pages_.empty() ? S_FALSE : S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetString(UINT index, BSTR* out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (index >= strings_.size()) return E_INVALIDARG;
        *out = SysAllocStringLen(strings_[index].data(), static_cast<UINT>(strings_[index].size()));
        return *out ? S_OK : E_OUTOFMEMORY;
    }
    HRESULT STDMETHODCALLTYPE GetPageIndex(UINT* out, UINT size, UINT* count) override {
        if (!count || (!out && size)) return E_POINTER;
        *count = static_cast<UINT>(pages_.size());
        if (out) std::copy_n(pages_.begin(), std::min<std::size_t>(size, pages_.size()), out);
        return size < pages_.size() ? S_FALSE : S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetPageIndex(UINT* pages, UINT count) override {
        if (!pagination_) return TF_E_DISCONNECTED;
        if (!pages || !count || count > strings_.size()) return E_INVALIDARG;
        try { return pagination_(std::vector<std::size_t>(pages, pages + count)); }
        catch (...) { return E_OUTOFMEMORY; }
    }
    HRESULT STDMETHODCALLTYPE GetCurrentPage(UINT* out) override {
        if (!out) return E_POINTER;
        *out = page_; return S_OK;
    }
    void update(const Session& session) {
        std::vector<std::wstring> strings;
        for (const auto& candidate : session.candidates()) {
            const char32_t cp = candidate.character;
            strings.push_back(utf16(std::u32string_view(&cp, 1)));
        }
        std::vector<UINT> pages;
        for (const auto start : session.candidate_pages()) pages.push_back(static_cast<UINT>(start));
        strings_ = std::move(strings); pages_ = std::move(pages);
        page_ = static_cast<UINT>(session.snapshot().page_index);
    }
    void detach() noexcept { visibility_ = {}; pagination_ = {}; document_.Reset(); shown_ = false; }
private:
    std::atomic<ULONG> refs_{1};
    Microsoft::WRL::ComPtr<ITfDocumentMgr> document_;
    Visibility visibility_;
    Pagination pagination_;
    std::vector<std::wstring> strings_;
    std::vector<UINT> pages_;
    UINT page_{};
    bool shown_{};
};
}
