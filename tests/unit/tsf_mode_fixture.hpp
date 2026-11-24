// Isolated TSF compartments and shell item manager. No Windows settings change.
struct ModeCompartment final : ITfCompartment, ITfSource {
    REFS;
    LONG flags{TF_CONVERSIONMODE_NATIVE | TF_CONVERSIONMODE_FULLSHAPE};
    ComPtr<ITfCompartmentEventSink> sink;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (iid == IID_IUnknown || iid == IID_ITfCompartment) *out = static_cast<ITfCompartment*>(this);
        else if (iid == IID_ITfSource) *out = static_cast<ITfSource*>(this);
        else return E_NOINTERFACE;
        AddRef(); return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetValue(TfClientId, const VARIANT* value) override {
        if (!value || value->vt != VT_I4) return E_INVALIDARG;
        flags = value->lVal;
        const auto notified = sink;
        if (notified) return notified->OnChange(GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetValue(VARIANT* value) override {
        if (!value) return E_POINTER;
        VariantInit(value); value->vt = VT_I4; value->lVal = flags; return S_OK;
    }
    HRESULT STDMETHODCALLTYPE AdviseSink(REFIID iid, IUnknown* value, DWORD* cookie) override {
        if (iid != IID_ITfCompartmentEventSink) return E_NOINTERFACE;
        *cookie = 1;
        return value->QueryInterface(IID_PPV_ARGS(&sink));
    }
    HRESULT STDMETHODCALLTYPE UnadviseSink(DWORD) override { sink.Reset(); return S_OK; }
};
struct ModeInterfaces final : ITfCompartmentMgr, ITfLangBarItemMgr, ITfLangBarItemSink {
    REFS;
    ModeCompartment conversion;
    ComPtr<ITfLangBarItem> item;
    DWORD item_cookie{TF_INVALID_COOKIE}, changed{};
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (iid == IID_IUnknown || iid == IID_ITfCompartmentMgr) *out = static_cast<ITfCompartmentMgr*>(this);
        else if (iid == IID_ITfLangBarItemMgr) *out = static_cast<ITfLangBarItemMgr*>(this);
        else if (iid == IID_ITfLangBarItemSink) *out = static_cast<ITfLangBarItemSink*>(this);
        else return E_NOINTERFACE;
        AddRef(); return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetCompartment(REFGUID id, ITfCompartment** out) override {
        *out = nullptr;
        if (id != GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION) return E_INVALIDARG;
        *out = &conversion; conversion.AddRef(); return S_OK;
    }
    HRESULT STDMETHODCALLTYPE AddItem(ITfLangBarItem* value) override {
        item = value;
        ComPtr<ITfSource> source;
        const auto result = item.As(&source);
        if (FAILED(result)) return result;
        return source->AdviseSink(IID_ITfLangBarItemSink, static_cast<ITfLangBarItemSink*>(this), &item_cookie);
    }
    HRESULT STDMETHODCALLTYPE RemoveItem(ITfLangBarItem* value) override {
        if (item.Get() != value) return E_INVALIDARG;
        ComPtr<ITfSource> source;
        if (SUCCEEDED(item.As(&source))) (void)source->UnadviseSink(item_cookie);
        item.Reset(); item_cookie = TF_INVALID_COOKIE; return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetItem(REFGUID id, ITfLangBarItem** out) override {
        *out = nullptr;
        if (id != GUID_LBI_INPUTMODE) return E_INVALIDARG;
        return item.CopyTo(out);
    }
    HRESULT STDMETHODCALLTYPE OnUpdate(DWORD flags) override { changed |= flags; return S_OK; }
    void external_mode(bool english) {
        VARIANT value; VariantInit(&value);
        value.vt = VT_I4;
        value.lVal = TF_CONVERSIONMODE_FULLSHAPE | (english ? 0 : TF_CONVERSIONMODE_NATIVE);
        check(conversion.SetValue(1, &value) == S_OK, "External conversion compartment update");
    }
    bool english() const { return !(conversion.flags & TF_CONVERSIONMODE_NATIVE); }
    std::wstring label() {
        ComPtr<ITfLangBarItemButton> button;
        check(item && item.As(&button) == S_OK, "Shell has a mode button");
        BSTR value{};
        check(button->GetText(&value) == S_OK, "Read mode label");
        std::wstring result(value); SysFreeString(value); return result;
    }
    STUB(ClearCompartment, (TfClientId, REFGUID))
    STUB(EnumCompartments, (IEnumGUID**))
    STUB(EnumItems, (IEnumTfLangBarItems**))
    STUB(AdviseItemSink, (ITfLangBarItemSink*, DWORD*, REFGUID))
    STUB(UnadviseItemSink, (DWORD))
    STUB(GetItemFloatingRect, (DWORD, REFGUID, RECT*))
    STUB(GetItemsStatus, (ULONG, const GUID*, DWORD*))
    STUB(GetItemNum, (ULONG*))
    STUB(GetItems, (ULONG, ITfLangBarItem**, TF_LANGBARITEMINFO*, DWORD*, ULONG*))
    STUB(AdviseItemsSink, (ULONG, ITfLangBarItemSink**, const GUID*, DWORD*))
    STUB(UnadviseItemsSink, (ULONG, DWORD*))
};
