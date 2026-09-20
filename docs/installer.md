# 安裝包 0.4.0

0.4.0 使用「錦筆劃輸入法」，WinUI 3 設定程式與捷徑名為「錦筆畫輸入法設定」。升級維持原 AppId／安裝目錄與設定資料，開始功能表使用新群組。

給使用者：傳送 `out/release/StrokeIME-0.4.0-win64.zip`，解壓後双擊 `StrokeIME-Setup-0.4.0-win64.exe`。需要管理員核准，之後按 Win+空白選「錦筆劃輸入法」。使用說明同包附上，也放在開始功能表。

支援 x64 Windows 10 1809 以上／11，同包安裝 x64 與 x86 DLL；不接受 ARM64 或 32 位元 Windows。Windows 11 本機測試不代表所有電腦／宿主相容。安裝包尚未簽章，不繞過 Windows 安全機制。

## 可重建流程

工具：PowerShell 7、CMake、Visual Studio Build Tools 2026 C++／Windows SDK、Inno Setup 6.7.3。先按 dictionary.md 產生 traditional 字庫包，再執行：

```powershell
./packaging/build.ps1
```

可用 `-Iscc '編譯器完整路徑'` 指定 Inno Setup。script 使用 package-x64／package-x86 presets，獨立建置且靜態連結 MSVC runtime，執行兩架構完整 CTest，複製 DLL、setup 與完整字庫授權包，再編譯 installer、產生 SHA256。不會安裝產品或註冊輸入法。

產品程式仍為 C++20。`.iss` 與 `.isl` 僅是 Inno Setup 安裝描述／介面字串，PowerShell 僅負責建置。沒有新增產品執行時的 Python、.NET 或第三方套件需求。

## 安裝與更新

- 固定 AppId，機器層安裝到 Program Files/StrokeIME；個別 payload 放 versions/0.3.1/{x64,x86}。
- 安裝先做兩架構 COM selftest，再以官方 TSF API 註冊。失敗嘗試解除部分註冊並恢復安裝前 DLL 的註冊，再交 Inno 清理檔案；恢復錯誤會寫入安裝 log。
- 成功後以原本使用者執行 enable，不更改預設輸入法。若原使用者 token 不可取得，提供「啟用筆劃輸入法」捷徑，供實際使用帳號操作。
- 更新需提高 package AppVersion 並使用新的版本目錄；舊 DLL 可留給已開啟程序。新版本與舊版本共用 AppId，解除安裝時一起清理 installer 管理過的檔案。
- 不強制關閉任何應用程式、不自動重新開機。更新或移除前使用者應儲存文件、切到其他輸入法，之後重開使用中的程式。
- 解除安裝只在本專案 COM 註冊仍指向本安裝目錄時解除註冊，避免刪除另外部署的開發版註冊。被占用檔案交 Windows 於重開機時移除。
- 多使用者：enable／disable 屬於目前帳號，其他帳號的清單可能需各自移除；不載入或批次修改其他使用者的 registry hive。

第一次正常執行安裝程式即可，不必特地「以其他使用者身分」啟動。安裝 log 在使用者 Temp（或以 `/LOG=完整路徑` 指定）。

## 測試入口

```powershell
# 安裝後一般使用者
& 'C:/Program Files/StrokeIME/versions/0.3.1/x64/stroke_setup.exe' status
& 'C:/Program Files/StrokeIME/versions/0.3.1/x64/stroke_setup.exe' verify-registered
```

verify-registered 需要先在 Windows 選到本輸入法；它不是端到端打字測試。驗證歷史與已知限制見 validation.md。

Inno 參考：[檔案部署](https://jrsoftware.org/ishelp/topic_filessection.htm)、[原使用者執行](https://jrsoftware.org/ishelp/topic_isxfunc_execasoriginaluser.htm)、[架構篩選](https://jrsoftware.org/ishelp/topic_archidentifiers.htm)。

WinUI 設定介面建置後會隨整個 settings 目錄部署，包括 app-local Windows App SDK、CRT、PRI 與 licenses；開始功能表捷徑改指向該目錄。最低 OS 為 Windows 10 1809（10.0.17763），符合 WinUI 依賴要求。0.4.0 起使用此部署方式。

