#pragma once
#include "runtime.hpp"
#include <wrl/client.h>
#include <functional>

namespace stroke::win {
// The host owns a reference while queued. Each callback executes at most once.
class WriteSession final : public ITfEditSession {
public:
    using Work = std::function<HRESULT(TfEditCookie)>;
    using Begin = std::function<bool()>;
    using Finish = std::function<void(HRESULT)>;
    using Inserted = std::function<void(ITfRange*)>;
    using Position = std::function<bool(TfEditCookie)>;
    WriteSession(ITfContext* context, std::wstring text, Begin begin, Finish finish,
                 Inserted inserted = {}, Position position = {})
        : context_(context), text_(std::move(text)), begin_(std::move(begin)), finish_(std::move(finish)), inserted_(std::move(inserted)), position_(std::move(position)) { ++live_objects; }
    WriteSession(ITfContext* context, Work work, Begin begin, Finish finish)
        : context_(context), begin_(std::move(begin)), finish_(std::move(finish)), work_(std::move(work)) { ++live_objects; }
    ~WriteSession() { --live_objects; }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (iid != IID_IUnknown && iid != IID_ITfEditSession) return E_NOINTERFACE;
        *out = static_cast<ITfEditSession*>(this); AddRef(); return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override { const auto r = --refs_; if (!r) delete this; return r; }
    HRESULT STDMETHODCALLTYPE DoEditSession(TfEditCookie cookie) override {
        if (called_ || finished_) return E_UNEXPECTED;
        called_ = true;
        HRESULT hr = E_FAIL;
        try {
            if (!begin_()) hr = TF_E_DISCONNECTED;
            else if (work_) hr = work_(cookie);
            else if (position_ && !position_(cookie)) hr = TF_E_DISCONNECTED;
            else {
                Microsoft::WRL::ComPtr<ITfInsertAtSelection> inserter;
                hr = context_.As(&inserter);
                if (SUCCEEDED(hr)) {
                    Microsoft::WRL::ComPtr<ITfRange> range;
                    hr = inserter->InsertTextAtSelection(cookie, 0, text_.data(), static_cast<LONG>(text_.size()), &range);
                    if (SUCCEEDED(hr)) {
                        // A caret failure cannot undo a successful insertion or justify retrying it.
                        bool placed = false;
                        if (range && SUCCEEDED(range->Collapse(cookie, TF_ANCHOR_END))) {
                            TF_SELECTION selection{range.Get(), {TF_AE_NONE, FALSE}};
                            placed = SUCCEEDED(context_->SetSelection(cookie, 1, &selection));
                        }
                        // Observation failures must never turn an inserted character into a retry.
                        if (inserted_) { try { inserted_(placed ? range.Get() : nullptr); } catch (...) {} }
                        hr = S_OK;
                    }
                }
            }
        } catch (...) { hr = E_FAIL; }
        finish(hr); return hr;
    }
    void finish(HRESULT hr) noexcept {
        if (finished_) return;
        finished_ = true;
        try { finish_(hr); } catch (...) {}
    }
    // Prefer the existing synchronous fast path; only retry when no write callback ran.
    void request(TfClientId client) noexcept {
        HRESULT result = E_FAIL;
        HRESULT hr = context_->RequestEditSession(client, this, TF_ES_SYNC | TF_ES_READWRITE, &result);
        if (!called_ && (hr == TF_E_SYNCHRONOUS || (SUCCEEDED(hr) && result == TF_E_SYNCHRONOUS))) {
            result = E_FAIL;
            hr = context_->RequestEditSession(client, this, TF_ES_ASYNC | TF_ES_READWRITE, &result);
        }
        if (!called_ && !(SUCCEEDED(hr) && result == TF_S_ASYNC))
            finish(FAILED(hr) ? hr : (FAILED(result) ? result : E_UNEXPECTED));
    }
private:
    std::atomic<ULONG> refs_{1};
    Microsoft::WRL::ComPtr<ITfContext> context_;
    std::wstring text_;
    Begin begin_;
    Finish finish_;
    Work work_;
    Inserted inserted_;
    Position position_;
    bool called_{}, finished_{};
};
}
