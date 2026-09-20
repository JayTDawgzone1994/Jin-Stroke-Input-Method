# 工作列識別圖示「劃」

開發版已將深色底、白色「劃」字製作為輸入法的 branding icon。它代表選取的輸入法，不是中英文模式按鈕；按住 Shift 暫時輸入英文不會換圖示。

- `src/windows/tsf/resources/stroke.ico`：16、20、24、32、40、48、64、256 像素的 32-bit 圖示。
- `stroke.rc`：把圖示嵌入 x64／x86 輸入法 DLL，resource ID 101。
- `resource.h`：圖示群組固定為 DLL 的第一個，profile icon index 為 0；日後新增其他圖示不可插到前面。
- `registration.cpp`：RegisterProfile 指向該 DLL 及圖示 index；註冊前檢查資源存在。
- `stroke_setup selftest`：檢查實際 DLL 的八種尺寸可由 LoadImage 載入。

此更動未安裝到使用者系統。需新版安裝／重新註冊，且 Windows 可能保留圖示快取；安裝後先切到其他輸入法再切回，必要時重新登入。Windows 控制圖示呈現位置與大小，本輪沒有宣稱已在真實工作列驗收。

圖示可用本專案純 C++ 離線工具重建，使用 Windows 的 Microsoft JhengHei UI 字型繪製字形，沒有打包或散布字型檔，也不需要執行期載入外部圖片：

```powershell
cmake --build out/build/package-x64 --config Release --target stroke_icon_builder
./out/build/package-x64/tools/icon_builder/Release/stroke_icon_builder.exe src/windows/tsf/resources/stroke.ico out/stroke-icon-preview.png
```

重建後需再編譯 DLL；圖示產物已放入原始碼，平常建置不會自動重繪。

依據：[Microsoft IME branding icon](https://learn.microsoft.com/en-us/windows/apps/develop/input/input-method-editor-requirements#ime-branding-icon)、[RegisterProfile](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nf-msctf-itfinputprocessorprofilemgr-registerprofile)。

## 0.4.1：修正系統匣仍顯示「繁體」

0.4.0 有嵌入圖示與 RegisterProfile icon index，但漏註冊 GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT。
依 Microsoft 的 IME system tray 規格，不相容輸入法會顯示語言縮寫，而非 branding icon。
已在安裝及解除安裝路徑對稱加入／移除此 category；沿用現有的「劃」圖示，不新增模式按鈕。
更改需重新註冊，Windows shell 也可能快取舊狀態；切換輸入法或重新登入後再驗收。
來源：https://learn.microsoft.com/en-us/windows/apps/develop/input/input-method-editor-requirements#ime-must-be-compatible-with-the-system-tray
