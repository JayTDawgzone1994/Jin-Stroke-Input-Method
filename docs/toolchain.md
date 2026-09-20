# Windows 開發工具鏈

安裝日期：2026-09-17。使用官方 winget 套件 `Microsoft.VisualStudio.BuildTools`，版本 18.10.1。安裝成功，不需重新開機。

## 安裝內容

- Visual Studio Build Tools 2026 18.10.1（安裝版本 18.10.12210.168）。
- MSVC x64／x86：工具目錄 14.51.36231；CMake 偵測編譯器 19.51.36257.0。
- Windows 11 SDK：10.0.26100.0。
- 已確認 msctf.h、x64／x86 Ole32.Lib，以及 rc.exe、mt.exe、signtool.exe。
- 元件清單保存於根目錄 `.vsconfig`，可在其他電腦匯入 Visual Studio Installer。

安裝目錄：`C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools`。
SDK：`C:\Program Files (x86)\Windows Kits\10`。

使用 CMake 的 Visual Studio generator 時不必把 cl.exe 永久加進 PATH，也不用改系統環境變數。原本 CMake、MinGW 與 Python 均保留。

## 驗證結果

- 現有完整專案：MSVC x64 Release 與 x86 Release 均建置成功，各 9 項 CTest 全部通過。
- x64 測試 3.87 秒，x86 4.44 秒；均包含真實 Conway 字庫及核心流程。這是整套測試耗時，不是單次按鍵延遲。
- TSF／SDK probe：兩個架構皆成功編譯、連結及執行。
- MSVC Debug 尚未跑；Windows 輸入法本體與宿主相容性仍是下一階段。

## 一般建置方式

在專案根目錄 PowerShell 執行：

```powershell
cmake --preset msvc-x64
cmake --build --preset msvc-x64-release --parallel 1
ctest --preset msvc-x64-release
```

32 位元改用 `msvc-x86` 與 `msvc-x86-release`。Debug presets 已提供，本次工具驗證以 Release 為主。MSVC 可直接使用中文專案路徑，不需要 MinGW 的 V: 映射。

## 本機自動化環境的啟動注意事項

此次 Codex 命令環境帶有不同大小寫的 `Path`／`PATH` 重複項，MSBuild 的 .NET Framework 工具任務會因重複 key 失敗。只刪除並重設 `$env:PATH` 沒有解決；最後採用「在子程序環境中以不區分大小寫的字典去重」成功，沒有修改永久環境設定。

若重現同一問題，可以在 PowerShell 定義下列開發用函式；這是啟動工具的 shell 操作，不是產品的程式邏輯：

```powershell
function Invoke-CleanBuildTool {
    param([string]$Executable, [string[]]$Arguments)
    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $Executable
    $startInfo.WorkingDirectory = (Get-Location).Path
    $startInfo.UseShellExecute = $false
    $cleanEnvironment = [System.Collections.Generic.Dictionary[string,string]]::new(
        [System.StringComparer]::OrdinalIgnoreCase)
    foreach ($entry in [System.Environment]::GetEnvironmentVariables('Process').GetEnumerator()) {
        $cleanEnvironment[$entry.Key] = $entry.Value
    }
    $startInfo.Environment.Clear()
    foreach ($entry in $cleanEnvironment.GetEnumerator()) {
        $startInfo.Environment[$entry.Key] = $entry.Value
    }
    foreach ($argument in $Arguments) { $startInfo.ArgumentList.Add($argument) }
    $process = [System.Diagnostics.Process]::Start($startInfo)
    $process.WaitForExit()
    if ($process.ExitCode -ne 0) { throw "Build tool failed: $($process.ExitCode)" }
}
Invoke-CleanBuildTool 'C:\Program Files\CMake\bin\cmake.exe' @('--preset', 'msvc-x64')
Invoke-CleanBuildTool 'C:\Program Files\CMake\bin\cmake.exe' @('--build', '--preset', 'msvc-x64-release', '--parallel', '1')
Invoke-CleanBuildTool 'C:\Program Files\CMake\bin\ctest.exe' @('--preset', 'msvc-x64-release')
```

此處 MSBuild 多節點建置曾出現「0 errors 但整體失敗」；改用 `--parallel 1` 後可繼續，故本機自動化先用單節點。這是觀察到的啟動環境限制，不表示 C++ 核心不支援多執行緒或工具安裝失敗。

## SDK／TSF 診斷

`tools/sdk_probe` 是獨立診斷專案，檢查 C++20、TSF interfaces、COM／GUID 連結與 Windows resource compiler，沒有註冊輸入法、建立 TSF 服務或更改預設輸入法。

```powershell
cmake -S tools/sdk_probe -B out/build/sdk-probe-x64 -G 'Visual Studio 18 2026' -A x64
cmake --build out/build/sdk-probe-x64 --config Release --parallel 1
& '.\out\build\sdk-probe-x64\Release\stroke_sdk_probe.exe'
```

x86 使用另一個 build 目錄 `sdk-probe-x86` 與 `-A Win32`。兩個架構的診斷程式已編譯、連結、執行成功，輸出對應 64-bit／32-bit 與 `TSF headers/imports/GUIDs/resource OK`。

TSF 是 COM 介面；本檢查使用 SDK 的 `ole32`、`uuid`。SDK 沒有可直接連結的 `msctf.lib`，不應將不存在的庫列入相依。微軟也建議以 CoCreateInstance 建立 thread manager，而不是直接使用 TF_CreateThreadMgr：https://learn.microsoft.com/en-us/windows/win32/api/msctf/nf-msctf-tf_createthreadmgr 。診斷只檢查符號與資源，沒有實際建立 thread manager。

## 設定 App 的 WinUI 建置

設定 App 已改用純 C++/WinRT + WinUI 3，使用獨立 MSBuild 專案，沒有 C# App 或 .NET 執行時依賴。首次執行 apps/settings/winui/build.ps1 -Restore；之後 build.ps1 可使用已還原依賴離線建置。預設 CMake all 仍只建置核心與 TSF，需要設定程式時使用 stroke_settings target 或上述腳本。依賴、測試及 app-local 部署見 [winui-settings-build.md](winui-settings-build.md)。
