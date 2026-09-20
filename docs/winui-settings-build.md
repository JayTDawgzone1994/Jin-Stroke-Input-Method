# WinUI 3 設定程式建置與驗證

工具：Visual Studio 2026 C++ Build Tools（v145）、Windows SDK 10.0.26100.0、PowerShell 7。
維持 C++20；沒有 C# 或 .NET 桌面 App。主介面為 WinUI 3 + XAML。

首次明確還原依賴：

```powershell
./apps/settings/winui/build.ps1 -Restore
```

之後可離線建置與測試：

```powershell
./apps/settings/winui/build.ps1
./apps/settings/winui/test.ps1
```

測試資料放在 `out/tests/settings-winui/<隨機識別>`，不接觸使用者設定。
測試建立實際 WinUI 視窗，驗證完整打字區、保留鍵、筆畫選單、預設配置、多鍵共用筆畫與自訂保留、筆劃預覽、反轉旗標、
設定讀寫、學習開關與深淺色切換，並輸出深色、淺色、窄視窗的 BMP 快照。真人操作仍須驗收 IME 啟用時的鍵盤流程。

產物：`out/build/settings-winui/x64/Release/stroke_settings.exe`。
必須連同同目錄全部依賴散布；PDB／ILK 可排除。`build.ps1` 複製 MSVC x64
CRT redistributables，保留 NuGet license／NOTICE／nuspec。依賴固定於
`apps/settings/winui/packages.lock.json`，來源為官方 NuGet。

開發預覽可指定隔離資料及主題：

```powershell
stroke_settings.exe --preview "D:\test-state" --theme dark
stroke_settings.exe --preview "D:\test-state-light" --theme light
```

省略預覽參數才會讀取 LocalAppData/StrokeIME 的正式設定，並跟隨 OS 主題。
供 GUI 測試使用，若將 EXE 複製為 `stroke_settings_preview.exe` 或
`stroke_settings_dark_preview.exe`，會自動改用旁邊的 preview-light／preview-dark
資料夾；必要時同時複製 PRI 為相同 basename。這些別名不放進發布包。

安裝腳本已改用獨立的 settings 子目錄及新捷徑。此次不升版、不覆蓋 0.3.1
發布檔，也不安裝到系統；正式發布前需要新版本號與乾淨 Windows 環境測試。
