# 錦筆劃輸入法 · Jin Stroke Input Method

以 C++20 開發的 Windows 繁體中文筆畫輸入法，透過 Text Services Framework（TSF）整合到系統輸入法。

目前版本 **0.4.4**，主要在 Windows 11 x64 驗證，提供 x64 與 x86 TSF 元件。仍屬早期版本，特殊應用程式與遊戲的相容性需要持續測試。

## 功能

- 橫、豎、撇、點、折五類筆畫，以及萬用筆畫。
- 左手 `QWE / ASD`、右手 `UIO / JKL`；提供兩種預設配置與自訂鍵位。
- 常用字排序、本機使用頻率學習、反轉候選選字數字順序。
- 候選框跟隨系統深淺色，支援圓角；左右方向鍵或 Page Up / Page Down 翻頁。
- 按住 Shift 暫時輸入英文，大小寫依 Caps Lock；保留宿主的組合快捷鍵。
- WinUI 3 設定程式「錦筆畫輸入法設定」。
- TSF UILess 候選介面與非同步文字提交路徑。

使用說明見 [快速入門](packaging/quick-start.txt)。安裝後按 `Win + 空白` 切換，從開始功能表開啟「錦筆畫輸入法設定」。本機偏好與學習資料存於 `%LOCALAPPDATA%\StrokeIME`，不納入原始碼或發布包。

## 從原始碼建置

需要 Windows、PowerShell 7 (`pwsh`)、Git、CMake，以及 Visual Studio 2026 Build Tools 的 C++ 桌面工具（v145）與 Windows SDK 10.0.26100.0。CMake 必須支援 `Visual Studio 18 2026` generator。設定程式另需 NuGet 網路存取；版本固定在 `apps/settings/winui/packages.lock.json`。製作安裝包需要 Inno Setup 6.7.3。

在專案根目錄執行，**先產生字庫，再建置 TSF**：

```powershell
git clone https://github.com/JayTDawgzone1994/Jin-Stroke-Input-Method.git
cd Jin-Stroke-Input-Method
cmake --preset package-x64
cmake --build out/build/package-x64 --config Release --target stroke_dict_builder
$builder = './out/build/package-x64/tools/dict_builder/Release/stroke_dict_builder.exe'
& $builder build --source data/upstream/conway --output data/generated/conway-v2.0.2-traditional --scope traditional --overrides data/overrides/project.tsv
& $builder frequency --source data/upstream/libchewing-data --output data/generated/conway-v2.0.2-traditional/frequency
cmake --build out/build/package-x64 --config Release
ctest --test-dir out/build/package-x64 -C Release --output-on-failure
pwsh -NoProfile -File apps/settings/winui/build.ps1 -Restore
```

字庫工具刻意拒絕覆寫既有輸出目錄。上面兩個資料生成命令只需在首次建置時執行；重新生成請先將舊輸出另存，再使用空的輸出路徑。上游快照已包含在儲存庫，不需要下載浮動版本。一般編譯與測試不會註冊輸入法。

完成字庫後，可建立包含 x64、x86、設定程式與授權文件的安裝包：

```powershell
pwsh -NoProfile -File packaging/build.ps1
```

輸出位於 `out/release/`。安裝需要系統管理員權限；更新後，仍載有舊 DLL 的應用程式可能需要重新開啟。設定介面自動測試另見 [WinUI 建置說明](docs/winui-settings-build.md)。

## 架構與開發文件

| 目錄 | 用途 |
| --- | --- |
| `src/domain` | 筆畫、鍵位與資料型別 |
| `src/dictionary` | 字庫索引、字頻資料與查詢 |
| `src/engine` | 輸入狀態、候選排序與翻頁 |
| `src/windows` | TSF、候選視窗、本機儲存 |
| `apps/settings/winui` | C++/WinRT 設定介面 |
| `tools` | 字庫轉換、圖示產生與查字工具 |
| `tests` | 核心及 Windows 整合測試 |
| `data/upstream` | 固定版本的上游資料與授權 |
| `data/overrides` | 專案筆畫修正 |

閱讀 [架構](docs/architecture.md)、[字庫](docs/dictionary.md)、[字頻](docs/frequency.md)、[學習](docs/learning.md)、[相容性](docs/tsf-compatibility.md) 與 [貢獻指南](CONTRIBUTING.md)。`docs/validation.md` 和研究筆記保留各開發階段的歷史紀錄；其中舊版本的限制或測試數量不代表目前狀態。

## 授權與致謝

自行開發的程式碼採 [MIT License](LICENSE)。第三方內容各自保留原授權：

- [Conway Stroke Data](https://github.com/stroke-input/stroke-input-data)：CC BY 4.0，提供筆畫資料。
- [libchewing-data](https://codeberg.org/chewing/libchewing-data)：LGPL-2.1-or-later，提供單字排序分數；未連結新酷音引擎。
- WinUI / Windows App SDK 與執行階段：依隨附套件條款。

完整來源、固定版本、修改說明與再散布文件見 [第三方聲明](THIRD_PARTY_NOTICES.md) 和 [來源備忘](note.md)。第三方字庫不因放入本專案而改為 MIT。
