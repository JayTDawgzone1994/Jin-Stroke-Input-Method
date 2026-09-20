# 第三方資料與發布備忘

更新日期：2026-09-20

這份文件追蹤需要標註來源的 library／字庫。它是開發備忘，不能取代安裝包內的完整授權與對應來源。

| 來源 | 用途與狀態 | 授權 | 發布時要保留的內容 |
| --- | --- | --- | --- |
| Conway Stroke Data v2.0.2，Conway（@yawnoc） | 已使用：漢字筆畫、查字索引 | CC BY 4.0 | 作者、來源連結、授權文件／連結、轉換與篩選等修改說明；現有生成字庫已有 NOTICE.txt |
| 新酷音 libchewing-data，libchewing Core Team | 已接入：抽取單字最高分，以獨立字頻表提供候選排序 | LGPL-2.1-or-later；tsi.csv 與 word.csv 檔頭均有明確聲明 | 版權、完整 LGPL 授權、修改與日期說明，以及與發布資料對應的可修改來源和轉換／建置工具；衍生資料依 LGPL 提供 |

## 來源位置

- Conway：https://github.com/stroke-input/stroke-input-data
  - 使用版本 commit：`d66ba5f5aa4cb6583883dfe8c14de553bb43616a`。
  - 專案原始資料：`data/upstream/conway/`。
  - 授權：https://creativecommons.org/licenses/by/4.0/
- 新酷音目前上游：https://codeberg.org/chewing/libchewing-data
  - 此次採用官方封存 GitHub 快照：https://github.com/chewing/libchewing-data
  - 固定 commit：`c44e81aef24b06f1509f19e1be54c99812d0c43f`（2026-03-22）。這是明確固定的評估快照，不宣稱為目前最新版本。
  - 專案保存位置：`data/upstream/libchewing-data/`。
  - `provenance.json` 記錄下載時間、原始 URL、大小及 SHA-256。
  - `source.zip` 保存該 commit 的完整來源；`tsi.csv`、`word.csv` 為方便評估另存的未修改原始檔。
  - `README.upstream.md` 是上游資料格式說明。
  - `COPYING.LGPL-2.1.txt` 取自新酷音引擎固定 commit `3c4a93aa03d574c7f011ff84e8a2437c2f79b2cf` 的 COPYING；資料的授權依據是 CSV 自身的 `dc:license` 聲明。

## 接上字頻排序前

- 2026-09-20 已將核心欄位改為 `frequency_score`：越大越優先、未評分為 0、同字重複候選取最高分。完整筆畫吻合仍優先。索引只讀寫格式 2；不支援其他格式，安裝包需一併更新字庫。新酷音分數已接入，詳見 docs/frequency.md。

- 先分析 tsi.csv 的單字分數與多讀音重複項，決定如何合併；word.csv 是注音映射，不直接當字頻表。
- 將字頻資料與 Conway 筆畫資料分開維護，保留各自來源、授權及重建流程。
- 決定常用程度與「完整筆畫吻合」的排序關係，測試常用字、罕用字及萬用筆畫。
- 發布轉換後的資料時，一併提供對應來源、修改說明及轉換工具，檢查安裝包是否實際包含所需聲明。下載快照本身不代表已完成未來整合的授權工作。
- 此次沒有引入或連結新酷音引擎；已更新開發版核心，尚未安裝或發布新版安裝包。

## 分數語意調整驗證（2026-09-20）

- MSVC Release x64、x86 各 13 項 CTest 全數通過；涵蓋分數降冪、同分碼點順序、重複字取最高分、0／uint32 最大值、不支援格式的拒絕與格式 2 往返。
- 已移除舊格式自動轉換程式，重新產生現用 Conway 字庫，並在打包流程加入來源與複製後的字庫驗證。
- 僅更新原始碼、建置產物與文件，沒有重新打包或安裝，現用 0.3.1 安裝包維持原樣。


- 移除相容層後重新驗證：x64／x86 各 13 項測試通過；兩份重新生成的正式字庫均可載入，兩架構選取「一」的 console 模擬提交通過。此步未重新發布安裝包。

## 新酷音字頻整合（2026-09-20）

- 已完成純 C++ 轉換、獨立字頻表載入及候選排序；完整吻合優先。同字多讀音取最高分。詳見 docs/frequency.md。
- 新增字頻檔案解析、損壞拒絕、不可變載入、排序、真實來源轉換、重現性與來源保存測試；最終 x64／x86 各 15 項測試全數通過。
- 六組實際 Session 前後比較候選總數均不變；兩架構開發版 DLL 旁的字库資料已更新，console 模擬選取 251 首字「口」通過。沒有做本輪 Windows 介面實測。
- 分數表 SHA-256：801D2A018BF88BD1CCE5651B1F7A104EC54EFCBD0EC9740F0DFE8949785B2A14。
- 已保存原始資料、完整授權及轉換工具來源；打包流程檢查必要來源，並更新 converter-source.zip。尚未製作或安裝新安裝包。


## 本地常用字學習（2026-09-20）

- 已實作 Session 成功輸出事件、漸進加分、Windows 程序間增量合併、generation 防止清除後復活，以及設定開關／清除按鈕。詳見 docs/learning.md。
- x64／x86 各 17 項測試通過，包含兩個獨立程序共 40 次並行更新及失敗重試。設定視窗已視覺確認。
- 本功能只有本專案 C++ 與 Windows API，沒有新增第三方 library 或授權來源。
- 開發版已編譯，未打包或安裝；已安裝的 0.3.1 尚無此功能。強制終止程序可能遺失尚未由 timer 同步的最後一批紀錄。


## 反轉選字鍵（2026-09-20）

- 按鍵設定增加反轉選字順序核取方塊，套用後下一次組字生效。候選標號及數字鍵共用 9→1 映射，候選排序和空白／Enter 首字行為不變。
- 已加入所有 0～9 候選頁長度的標號／選字一致性、短頁無效鍵、模式／自訂按鍵連同反轉旗標保存重載測試。
- 尚未製作或安裝新版安裝包。

## UILess 與非同步提交（2026-09-20）

- 已加入候選 UIElement 協商、完整候選／宿主分頁、UIELEMENTENABLED 註冊，以及同步遭拒後的非同步提交。取消與焦點等變動會使過期工作失效，成功插字才學習。
- x64／x86 各 18 項測試通過；最後的 UI 重入及空編輯修正再跑兩架構相關測試通過。詳細範圍見 docs/tsf-compatibility.md 與 docs/validation.md。
- 僅使用本專案 C++ 與 Windows SDK，沒有新增第三方 library 或授權來源。
- 尚未製作或安裝新安裝包；UILess 新類別必須隨新版註冊才會生效。仍需真實應用程式驗收，不能宣稱所有遊戲都相容。

## 工作列「劃」圖示（2026-09-20）

- 已新增八種尺寸的「劃」字識別圖示，內嵌到兩架構 TSF DLL；RegisterProfile 指向第一個圖示群組。詳見 docs/branding-icon.md。
- 圖示由純 C++ 離線工具配合 Windows GDI／GDI+ 產生；沒有新增第三方 library，也沒有散布字型檔。
- 尚未打包、安裝或在真實工作列驗收。

## 正式命名（2026-09-20）

- 產品名稱定為「錦筆劃輸入法」；設定程式依使用者指定名為「錦筆畫輸入法設定」，有意使用不同的「畫」字。
- 已更新輸入法 profile 顯示名、候選視窗／UILess 描述、設定視窗／提示／檔案描述、安裝與移除名稱、開始功能表捷徑及使用說明。
- 保留 StrokeIME 的 AppId、CLSID、profile GUID、EXE 名與資料路徑，確保既有安裝仍可升級，設定與學習繼續沿用。新安裝強制使用新群組名稱，清理舊預設群組的五個產品捷徑，只在空目錄時移除舊群組。
- 沒有新增第三方 library。尚未安裝或製作正式新版發布包。


## WinUI 3 設定介面（2026-09-20）

- 設定 App 改為 C++/WinRT + WinUI 3；Windows App SDK 僅隨獨立設定程式部署，TSF DLL 與候選視窗不引入 WinUI。
- 新增需保留來源／授權的依賴：Microsoft.WindowsAppSDK.WinUI 1.8.260803003、Foundation 1.8.260803002、InteractiveExperiences 1.8.260708001、Base 1.8.251216001；其 NuGet 二進位依 Microsoft Windows App SDK license.txt 與 NOTICE.txt，不可一律稱為 MIT。
- Microsoft.Web.WebView2 1.0.3179.45 為 WinUI 相依項；保留其 LICENSE.txt／NOTICE.txt。設定介面沒有使用 WebView，也不另外下載 Edge WebView2 Runtime。
- Microsoft.Windows.CppWinRT 2.0.250303.1 為 C++ 投影／建置工具，保留套件 LICENSE（MIT）。SDK BuildTools 與 MSIX BuildTools 為建置相依，詳見 packages.lock.json 與各 nuspec。
- 自帶 MSVC x64 CRT redistributables，取自 VS 官方 VC/Redist/MSVC 目錄；依 Visual Studio 可轉散發條款，不視為本專案自行授權的程式碼。
- build.ps1 將套件的 license／NOTICE／nuspec 收入設定程式 licenses 子目錄；發布需整包保留。官方來源：https://www.nuget.org/packages/Microsoft.WindowsAppSDK.WinUI/1.8.260803003 、https://www.nuget.org/packages/Microsoft.Windows.CppWinRT/2.0.250303.1 。
- 原 Win32 設定 UI 已移除，沿用既有 layout.dat 與 learning.dat，沒有新舊資料格式轉換。詳見 docs/winui-settings-build.md 與 docs/decisions/0007-winui-settings.md。

## 0.4.0 安裝包（2026-09-20）

已打包並依使用者要求升級本機到 0.4.0，正式設定與學習資料保持不變。包含新的 WinUI 設定介面、正式名稱、劃圖示、常用字排序／本機學習、反轉選字、UILess 與非同步提交等先前開發內容。完整驗證紀錄見 docs/validation.md。

## 0.4.2（2026-09-21）

候選框加入系統深淺色、圓角與細邊框，移除選字／翻頁操作教學；快捷鍵不變。已打包並更新本機。維持純 C++／Win32，沒有新增第三方 library。

## 0.4.3（2026-09-21）

候選組字中可用 ← 上一頁、→ 下一頁；PageUp／PageDown 保留，未組字和修飾键＋方向鍵交給宿主。已打包並安裝本機；沒有新增 library 或授權。

## 開源發布（2026-09-21）

- 自有程式碼採 MIT，見根目錄 LICENSE；第三方資料不變更授權。
- 完整來源彙整見 THIRD_PARTY_NOTICES.md；此文件前段保留歷史開發紀錄。
- 安裝包加入自有授權與第三方聲明，字頻轉換原始碼壓縮包也附上授權。
- Git 排除編譯產物、生成字庫、本機偏好、學習資料及憑證。
