// Synthetic text ranges for exercising real TSF adapter edit sessions.
struct CompositionRange final : ITfRange {
    ULONG refs{1};
    std::wstring* text;
    bool* fail;
    LONG start{}, end{};
    CompositionRange(std::wstring* value, bool* failure, LONG first, LONG last)
        : text(value), fail(failure), start(first), end(last) {}
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override {
        const auto r = --refs;
        if (!r)
            delete this;
        return r;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (!out)
            return E_POINTER;
        *out = nullptr;
        if (iid != IID_IUnknown && iid != IID_ITfRange)
            return E_NOINTERFACE;
        *out = static_cast<ITfRange*>(this);
        AddRef();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetText(TfEditCookie, DWORD, const WCHAR* value,
                                      LONG count) override {
        if (*fail)
            return E_FAIL;
        text->replace(static_cast<std::size_t>(start), static_cast<std::size_t>(end - start), value,
                      static_cast<std::size_t>(count));
        end = start + count;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Collapse(TfEditCookie, TfAnchor anchor) override {
        if (anchor == TF_ANCHOR_START)
            end = start;
        else
            start = end;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Clone(ITfRange** out) override {
        *out = new CompositionRange(text, fail, start, end);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE ShiftEnd(TfEditCookie, LONG count, LONG* moved,
                                       const TF_HALTCOND*) override {
        const auto target = std::clamp(end + count, start, static_cast<LONG>(text->size()));
        *moved = target - end;
        end = target;
        return S_OK;
    }
    STUB(GetText, (TfEditCookie, DWORD, WCHAR*, ULONG, ULONG*))
    STUB(GetFormattedText, (TfEditCookie, IDataObject**))
    STUB(GetEmbedded, (TfEditCookie, REFGUID, REFIID, IUnknown**))
    STUB(InsertEmbedded, (TfEditCookie, DWORD, IDataObject*))
    STUB(ShiftStart, (TfEditCookie, LONG, LONG*, const TF_HALTCOND*))
    STUB(ShiftStartToRange, (TfEditCookie, ITfRange*, TfAnchor))
    STUB(ShiftEndToRange, (TfEditCookie, ITfRange*, TfAnchor))
    STUB(ShiftStartRegion, (TfEditCookie, TfShiftDir, BOOL*))
    STUB(ShiftEndRegion, (TfEditCookie, TfShiftDir, BOOL*))
    STUB(IsEmpty, (TfEditCookie, BOOL*))
    STUB(IsEqualStart, (TfEditCookie, ITfRange*, TfAnchor, BOOL*))
    STUB(IsEqualEnd, (TfEditCookie, ITfRange*, TfAnchor, BOOL*))
    HRESULT STDMETHODCALLTYPE CompareStart(TfEditCookie, ITfRange* other, TfAnchor anchor,
                                           LONG* result) override {
        auto* range = dynamic_cast<CompositionRange*>(other);
        if (!range || range->text != text)
            return E_INVALIDARG;
        *result = start - (anchor == TF_ANCHOR_START ? range->start : range->end);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE CompareEnd(TfEditCookie, ITfRange* other, TfAnchor anchor,
                                         LONG* result) override {
        auto* range = dynamic_cast<CompositionRange*>(other);
        if (!range || range->text != text)
            return E_INVALIDARG;
        *result = end - (anchor == TF_ANCHOR_START ? range->start : range->end);
        return S_OK;
    }
    STUB(AdjustForInsert, (TfEditCookie, ULONG, BOOL*))
    STUB(GetGravity, (TfGravity*, TfGravity*))
    STUB(SetGravity, (TfEditCookie, TfGravity, TfGravity))
    STUB(GetContext, (ITfContext**))
};
struct HostComposition final : ITfComposition {
    ULONG refs{1};
    ComPtr<ITfRange> range;
    ComPtr<ITfCompositionSink> sink;
    std::function<void()> on_end;
    bool* fail_end{};
    bool ended{};
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override {
        const auto r = --refs;
        if (!r)
            delete this;
        return r;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (!out)
            return E_POINTER;
        *out = nullptr;
        if (iid != IID_IUnknown && iid != IID_ITfComposition)
            return E_NOINTERFACE;
        *out = static_cast<ITfComposition*>(this);
        AddRef();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetRange(ITfRange** out) override { return range.CopyTo(out); }
    HRESULT STDMETHODCALLTYPE EndComposition(TfEditCookie cookie) override {
        if (*fail_end)
            return E_FAIL;
        if (ended)
            return E_UNEXPECTED;
        ComPtr<HostComposition> lifetime(this);
        ended = true;
        auto target = sink;
        sink.Reset();
        if (target)
            target->OnCompositionTerminated(cookie, this);
        if (on_end)
            on_end();
        return S_OK;
    }
    STUB(ShiftStart, (TfEditCookie, ITfRange*))
    HRESULT STDMETHODCALLTYPE ShiftEnd(TfEditCookie, ITfRange*) override { return S_OK; }
};
