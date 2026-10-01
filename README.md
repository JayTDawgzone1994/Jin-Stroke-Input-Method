# 錦筆劃輸入法 · Jin Stroke Input Method

以 C++20 開發的 Windows 繁體中文筆畫輸入法，透過 Text Services Framework（TSF）整合到系統輸入法。用五類筆畫查字，也可選擇連續組字或逐字聯想；詞庫與個人學習都在本機運作。

`develop` 目前為 **0.9.0**。正式發布的版本與安裝包請見 [GitHub Releases](https://github.com/JayTDawgzone1994/Jin-Stroke-Input-Method/releases)，開發分支版本不代表已發布。主要在 Windows 11 x64 驗證，特殊應用程式與遊戲的相容性仍需持續測試。

## 安裝與更新

下載 Release 的 `StrokeIME-Setup-版本-win64.exe`，或解壓同版 ZIP 後執行安裝程式。安裝需要系統管理員權限。

- 安裝包支援 x64 Windows 10 1809 以上／Windows 11，同時包含 x64 與 x86 TSF 元件，供 64 位元及 32 位元應用程式使用；不支援 ARM64 或 32 位元 Windows。
- 安裝後按 `Win + 空白` 切到「錦筆劃輸入法」。從開始功能表開啟「錦筆畫輸入法設定」，調整鍵位與輸入模式後按「套用變更」。
- 更新可直接執行新版安裝包，保留個人設定與學習資料。更新後請重開使用輸入法的程式與設定視窗；仍載有舊 DLL 時可重新登入 Windows。

操作詳見 [快速入門](packaging/quick-start.txt)，部署與更新細節見 [安裝說明](docs/installer.md)。

## 筆畫與鍵位

提供「標準」「傳統」「自訂」三種配置。兩種預設皆使用 `QWE / ASD` 與 `UIO / JKL`；同一筆畫可綁多個字母，不限制使用哪隻手。

| 筆畫 | 標準配置 | 傳統配置 |
| --- | --- | --- |
| 一　橫／提 | A、J | Q、U |
| 丨　豎／豎鈎 | S、K | W、I |
| 丿　撇 | D、L | E、O |
| 丶　點／捺 | Q、U | A、J |
| フ　折 | W、I | S、K |
| ＊　任意筆畫 | E、O | D、L |

設定視窗顯示主打字區鍵盤，點選字母鍵即可更改或取消筆畫綁定，修改後自動切換為「自訂」。數字、空白等保留鍵維持原功能。筆畫標示使用粗體與系統強調色，另有試打區可預覽鍵位。

按住 `Shift` 暫時輸入英文，大小寫依 `Caps Lock`，宿主組合快捷鍵維持原功能。候選選字預設為 `1～9`，可在設定反轉為 `9～1`。

## 輸入模式

預設逐字輸入，連續模式與逐字聯想都可自行開啟。

| 模式 | 操作與行為 |
| --- | --- |
| 逐字輸入 | 輸入筆畫後用數字選字，立即送出。筆畫候選可用左右鍵或 Page Up／Down 翻頁。 |
| 逐字聯想 | 一字成功送出後，有建議才開啟聯想窗；用數字接著選下一字，或按筆畫鍵繼續查字。聯想時左右、空白、Enter 與 Backspace 交給原程式，並結束目前聯想。 |
| 連續輸入 | 未確認的字先顯示筆畫順序。空白接受目前候選並開始下一字，草稿保留組字底線；左右移動，下鍵開候選修字。候選開啟時 Enter 先確認字，否則送出整段。 |

連續模式會使用新酷音詞庫預選常見詞；手動選過的字固定保留。開啟連續模式會自動關閉逐字聯想；切回逐字後，如需聯想請重新開啟並套用。

候選框依內容調整大小，跟隨系統深淺色並支援圓角、高對比；設定程式使用 WinUI 3。TSF 支援由宿主管理候選的 UILess 介面，以及同步／非同步文字提交。

完整操作見 [連續輸入](docs/continuous-input.md) 與 [逐字聯想](docs/association.md)。

## 學習與個人詞庫

「學習常用字與聯想」預設開啟，只記錄成功輸出的字：

- 常選的單字會提高排序；同一前文常選的接字也會更優先。
- 相鄰的 2～8 字漢字片段成功輸入三次後，可加入個人聯想。連續模式只學最終成功送出的文字，草稿與取消的字不計次。
- 設定下方的「個人聯想詞庫」可手動新增姓名或常用詞，也能搜尋、移除及重新整理；詞語修改立即儲存。
- 關閉學習時保留資料，手動詞仍可使用。清除學習紀錄會移除自動詞與使用次數，保留手動詞。

個人詞目前用於**逐字聯想**；連續模式能教詞，整段預選仍使用固定詞庫。自動學習以字串片段計數，沒有語意斷詞。

偏好與學習資料只存於 `%LOCALAPPDATA%\StrokeIME`，不讀取應用程式已有文件，不傳送網路，也不納入原始碼或發布包。詳見 [本機學習](docs/learning.md) 與 [個人詞庫](docs/personal-words.md)。

## 從原始碼建置

需要 Windows、PowerShell 7 (`pwsh`)、Git、CMake，以及 Visual Studio 2026 Build Tools 的 C++ 桌面工具（v145）與 Windows SDK 10.0.26100.0。CMake 必須支援 `Visual Studio 18 2026` generator。設定程式另需 NuGet 網路存取；版本固定在 `apps/settings/winui/packages.lock.json`。製作安裝包需要 Inno Setup 6.7.3。

在專案根目錄執行，**先產生字庫，再建置 TSF**。以下以 `develop` 為例；要重建特定 Release，請改為 checkout 對應版本標籤。

```powershell
git clone https://github.com/JayTDawgzone1994/Jin-Stroke-Input-Method.git
cd Jin-Stroke-Input-Method
git switch develop
cmake --preset package-x64
cmake --build out/build/package-x64 --config Release --target stroke_dict_builder
$builder = './out/build/package-x64/tools/dict_builder/Release/stroke_dict_builder.exe'
& $builder build --source data/upstream/conway --output data/generated/conway-v2.0.2-traditional --scope traditional --overrides data/overrides/project.tsv
& $builder frequency --source data/upstream/libchewing-data --output data/generated/conway-v2.0.2-traditional/frequency
& $builder phrases --source data/upstream/libchewing-data --index data/generated/conway-v2.0.2-traditional/dictionary.sidx --output data/generated/conway-v2.0.2-traditional/phrases
cmake --build out/build/package-x64 --config Release
ctest --test-dir out/build/package-x64 -C Release --output-on-failure
pwsh -NoProfile -File apps/settings/winui/build.ps1 -Restore
pwsh -NoProfile -File apps/settings/winui/test.ps1
```

字庫工具刻意拒絕覆寫既有輸出目錄。上面三個資料生成命令只需在首次建置時執行；重新生成請先將舊輸出另存，再使用空的輸出路徑。上游快照已包含在儲存庫，不需要下載浮動版本。一般編譯與測試不會註冊輸入法。

完成字庫後，可建立包含 x64、x86、設定程式與授權文件的安裝包：

```powershell
pwsh -NoProfile -File packaging/build.ps1
```

安裝程式、ZIP 與 SHA256 輸出到 `out/release/`；打包流程執行兩架構完整 CTest，不會自動安裝。設定介面建置細節見 [WinUI 建置說明](docs/winui-settings-build.md)。

## 架構與開發文件

| 目錄 | 用途 |
| --- | --- |
| `src/domain` | 筆畫、鍵位與個人詞資料型別 |
| `src/dictionary` | 筆畫索引、字頻與詞語前綴查詢 |
| `src/engine` | 查字、排序、連續草稿與逐字聯想 |
| `src/windows` | TSF、候選視窗、設定與學習儲存 |
| `apps/settings/winui` | C++/WinRT 設定介面及個人詞管理 |
| `tools` | 字庫轉換、圖示產生與查字工具 |
| `tests` | 核心、Windows 整合及原生組字測試 |
| `data/upstream` | 固定版本的上游資料與授權 |
| `data/overrides` | 專案筆畫修正 |

0.9.0 的 MSVC Release x64／x86 各 22 項 CTest 通過，打包後 TSF COM selftest 與 WinUI 隔離測試通過；已檢視深淺色及縮小視窗。這些檢查不代表所有 Windows 宿主都已驗證；高對比仍需真人驗收。細節與歷史紀錄見 [驗證紀錄](docs/validation.md)。

開發文件：[架構](docs/architecture.md)、[筆畫字庫](docs/dictionary.md)、[字頻](docs/frequency.md)、[詞庫](docs/phrases.md)、[設定](docs/settings.md)、[TSF 相容性](docs/tsf-compatibility.md)。日常開發在 `develop`，正式發布由已驗證的變更合併到 `main`；提交方式見 [貢獻指南](CONTRIBUTING.md)。歷史文件中舊版本的限制或測試數量不代表目前狀態。

## 授權與致謝

自行開發的程式碼採 [MIT License](LICENSE)。第三方內容各自保留原授權：

- [Conway Stroke Data](https://github.com/stroke-input/stroke-input-data)：CC BY 4.0，提供筆畫資料。
- [libchewing-data](https://codeberg.org/chewing/libchewing-data)：LGPL-2.1-or-later，提供單字分數與 128,957 個詞語；未連結新酷音引擎。
- WinUI / Windows App SDK 與執行階段：依隨附套件條款。

完整來源、固定版本、修改說明與再散布文件見 [第三方聲明](THIRD_PARTY_NOTICES.md) 和 [來源備忘](note.md)。第三方字庫不因放入本專案而改為 MIT。
