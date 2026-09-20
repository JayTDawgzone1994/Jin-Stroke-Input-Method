## 2026-09-20：更名為錦筆劃輸入法（尚未發布）

- x64／x86 的 TSF、設定及 setup Release 編譯通過，兩架構 windows.com_contracts 通過。設定 EXE 的 FileDescription／ProductName 已讀回確認為「錦筆畫輸入法設定」／「錦筆劃輸入法」。
- Inno Setup 腳本編譯通過，包含新開始功能表群組、捷徑與舊預設捷徑清理。僅使用既有 payload 檢查腳本，產物 rename-validation-not-for-install.exe 不可作為正式更新包。
- 沒有執行安裝或實機升級；安裝身分與資料位置保持不變。歷史驗證紀錄保留原產品名。

## 2026-09-20：工作列「劃」識別圖示（尚未發布）

- x64／x86 Release 的 TSF DLL、setup 與純 C++ 圖示產生器均編譯通過。兩架構 windows.com_contracts 通過，包含從實際 DLL 載入八種尺寸的圖示。
- 已查看產生的圖示預覽，確認字為「劃」；重新產生與保存的 ICO 雜湊一致。圖示為深色底白字，內嵌到 DLL 第一個圖示群組，profile icon index 0。
- 本次是圖示資源與註冊參照變更，沒有重跑無關核心測試。尚未安裝、重新註冊或驗證 Windows 工作列實際呈現；完整說明見 branding-icon.md。

## 2026-09-20：UILess 與非同步提交（尚未發布）

- package-x64／package-x86 Release 建置通過（/W4 /WX），各 18／18 CTest 通過。最後針對空編輯通知、Show 重入與 BeginUIElement 內 Deactivate 的修正，兩架構 windows.tsf_host／windows.com_contracts 再次通過。
- windows.tsf_host 直接編譯同一份 service.cpp，以隔離儲存目錄及可控制 COM 宿主驗證 UILess-only 與一般啟用、完整清單、宿主自訂頁界、Show／IsShown 與 Begin／Update／End。宿主重複 Show(TRUE) 不會產生訊息迴圈，Begin 內停用仍會結束已建立的 UI element。
- 驗證同步成功／同步拒絕改非同步、延後提交、重複回呼不重寫、快速 A1A1 排序、Esc／焦點／唯讀／外部編輯使舊工作失效、失敗保留候選、Shift 英文及 Caps Lock 快照、Deactivate 後回呼不插字。成功的四次漢字提交才產生四次學習，英文與失敗不計數。
- 測試未註冊輸入法、未修改使用者字庫／配置／學習。未安裝或製作新安裝包，也未完成真實遊戲、搜尋框、Office 或瀏覽器的新版端到端驗收。
- 設計與限制見 [tsf-compatibility.md](tsf-compatibility.md)。

## 2026-09-20：0.3.1 Windows 搜尋相容性修正

- 問題：0.3.0 僅註冊 TIP_KEYBOARD，未宣告 IMMERSIVESUPPORT；候選 popup 沒有 owner，也沒有 Windows IME light-dismiss 事件。
- 修正：新增 IMMERSIVESUPPORT 註冊／解除；GetActiveView/GetWnd（失敗採 GetFocus）指定 owner；候選顯示／改變／隱藏時通知對應 IME 事件。
- x64／x86 Release 各 13／13 CTest 通過。
- 本機以 0.3.1 安裝包直接覆蓋升級 0.3.0，安裝 exit 0；兩架構 selftest／register exit 0；沒有強制關閉使用者程式或重新開機。
- 在實際使用者工作階段執行 activate 和兩架構 verify-registered 都成功（DLL loaded、Foreground key sink 0x0）。受限 shell 的 GetProfile／ActivateProfile 回傳 E_FAIL，不能作為真實桌面故障的依據。
- 系統註冊指向 Program Files/StrokeIME/versions/0.3.1；新增 Category {13A016DF-560B-46CD-947A-4C3AF1E0E35D}，並保留 TIP_KEYBOARD。
- 自動 UI 工具無法列出搜尋面板；使用者在本機 Windows 搜尋框切至五筆劃，以標準配置按 A、1，明確回報「能出現候選，也能輸出一」。搜尋框基本輸入已由使用者實測確認。未宣稱完成 UI-less／整合搜尋候選支援。
## 2026-09-17：0.3.0 可調整按鍵配置

- package-x64／package-x86 Release 各 13／13 CTest 通過，警告視為錯誤。
- 新增 settings.layouts：確認兩種預設所有左右手鍵位、自訂／留空／同筆劃別名、衝突拒絕、設定格式與損壞檔、原子更新、儲存失敗保留舊檔，以及引擎在 composing 禁止換配置、idle 允許更新。
- 原生視窗實測：建立標準自訂、重複 Q 顯示衝突並停用套用、留空恢復可套用、橫笔改 Z，試打 zsdqwe 得到「一丨丿丶フ＊」、恢復預設、切回標準並成功儲存。
- 小螢幕實測後加入工作區縮放，完整對話框及底部按鈕可見。
- 0.3.0 安裝包含 x64／x86 設定程式與開始功能表捷徑，以及更新的使用說明。
- 本次未安裝 0.3.0、未做新版 TSF 跨程序熱更新／記事本端到端驗證。現有系統部署仍為先前版本；使用新功能須安裝新版並重開宿主。
## 2026-09-17：0.2.2 Shift 暫時英文

- 左／右 Shift＋A–Z 直接提交英文，大小寫僅依 Caps Lock；放開恢復筆劃。
- 首個英文成功提交才取消筆劃；失敗保留查詢，消耗按鍵以防系統輸出相反大小寫。單按 Shift／Caps Lock 不取消。
- 新增獨立 keyboard_policy，窮舉 256 個虛擬鍵 × 32 種修飾鍵狀態，包含 Ctrl／Alt／Win、方向鍵、數字、符號。
- package-x64、package-x86 Release 均建置通過，各 12／12 CTest 通過；含既有 COM、引擎、萬用筆劃及完整字庫測試。
- 已產生 0.2.2 EXE、SHA256、ZIP；本次未安裝 0.2.2，未做實際記事本打字或升级安裝驗證，系統仍使用 0.2.1。
- 待實機確認：左右 Shift、Caps Lock 開／關、長按字母、兩種放鍵順序、未完成筆劃接英文、反白取代、Shift＋方向鍵及 Ctrl＋Shift 快捷鍵。
# 框架驗證紀錄

## 2026-09-17：可分發安裝包 0.2.1

- Inno Setup 6.7.3；packaging/build.ps1 完整執行，package-x64／package-x86 靜態 CRT 建置及各 11／11 CTest 通過。
- 四個交付的 DLL／EXE 的 PE imports 只有 Windows 系統 DLL，沒有 MSVCP／VCRUNTIME 外部依賴；靜態 CRT 的 DLL 保留執行緒通知。
- 封裝繁體中文主要精靈字串與說明、兩架構 payload、完整字庫 NOTICE／sources、解除安裝與開始功能表入口。
- Windows 11 x64 實際 silent 安裝，兩 COM registry view 指向 Program Files/StrokeIME/versions/0.2.1；啟用後兩架構 verify-registered 通過。
- 初次移除測試發現第二架構遇到共用 TSF profile 已被移除而報錯；修正為僅在確認服務 TIP registry 不存在時接受重複解除註冊，其餘失敗仍回報。
- 修正版安裝 → 移除通過，兩架構 unregister exit 0，產品目錄與兩 COM class key 均不存在，log 記錄 Removed all Yes、無須重新開機。
- 最後重新安裝與同版本修復安裝均 exit 0，兩架構再次啟用通過。目前保留安裝包版本；舊 workspace 開發包仍在，但不再是註冊目標。
- 交付 EXE、SHA256、使用說明與 ZIP，未數位簽章。未在其他使用者電腦或全新 VM 測試，未實測跨使用者 UAC、強制失敗回滾、未來不同版本升級、占用檔案需重開機的流程；ARM64 由安裝包拒絕。
- 紀錄在 out/package/{install-fixed,uninstall-fixed,install-final,repair-test}.log。此階段未重新操作記事本，打字驗證見前期紀錄。

## 2026-09-17：萬用筆劃與 QWEASD／UIOJKL

- x64／x86 Release 全部建置成功，/W4 /WX；各 11／11 CTest 通過（3.28／3.85 秒）。
- engine.wildcard 窮舉 1–4 token 的 1,554 種查詢，涵蓋 5 種實筆與萬用符，與獨立線性 oracle 比較匹配／去重／完整碼標記。
- 驗證萬用符恰好一筆、首筆及連續萬用、128 筆上限、非法 token、禁止存入字庫、12 個預設鍵位、左右手混用、選字提交、提交失敗保留萬用及退格。
- 使用真實 traditional 索引，15 次查詢（首筆萬用、32／128 個連續萬用各 5 次）共 38 ms，這是本機 dictionary 查詢計時，不是端到端按鍵延遲保證。
- console 真實字庫 `1*34` 顯示「木」為第 5 個完整長度匹配候選，另有其他相符字；沒有假設萬用查詢只能命中一字。
- 新部署位於 out/install/dev-0.2.0/{x64,x86}，兩種架構已更新 COM／TSF 註冊並通過 verify-registered；保留 dev-0.1.0 供現有程序繼續使用。
- 初次啟用檢查沒有前景接收器，診斷確認 DLL 未載入；先切到本輸入法後兩架構通過。setup 增加 STA 訊息處理、載入診斷與切換後重試提示，沒有將失敗誤判為通過。
- 本次未另操作記事本。候選符號顯示已編譯，查詢及按鍵映射經自動測試；舊程序可能仍使用舊鍵位，使用者應儲存文件後重開程式再試。

## 2026-09-17：最小 Windows TSF 整合

- MSVC 19.51、SDK 10.0.26100.0：x64／x86 Release DLL、setup EXE 建置通過，/W4 /WX。
- x64 CTest 10／10（2.78 秒）；x86 10／10（3.17 秒）。新增 windows.com_contracts 驗證 factory、介面、無效參數、引用釋放與 DllCanUnloadNow；不宣稱其涵蓋 Windows 啟用。
- 兩個架構都已註冊到 out/install/dev-0.1.0，並為目前使用者 enable；未設為預設輸入法。
- 兩個架構的 verify-registered 均通過：建立有焦點 context，以 FORPROCESS 啟用，GetForeground 確認為本專案 CLSID，最後停用 thread manager。
- 真實 Windows 11 記事本 11.2607.14.0、x64：使用者提供空白分頁後，W → 1 寫入「一」；I → L → U → 2 寫入「口」，文件共 2 字「一口」。這些是按鍵經 TSF 選字提交，非貼上 Unicode 或 console 模擬。
- 記事本確認 PageDown 從第 1 頁到第 2 頁、PageUp 返回；Backspace 退掉最後一筆關閉候選窗，既有文字仍為 2 字。
- 再輸入 U 後按 Esc，候選視窗關閉，文件仍為「一口」。
- 自動化先因既有私人文件的讀取被審核拒絕，改由使用者準備空白分頁後繼續，未讀取原文件内容。
- 本版尚未驗證 MSVC Debug、其他宿主／密碼欄位、強制寫入失敗的真實宿主場景、移除與重装流程。核心已有提交失敗恢復測試；Windows adapter 只採同步提交，不排隊。

以下為各階段歷史紀錄，較早的「未實作 TSF」不代表目前狀態。

## 2026-09-17：MSVC／Windows SDK 安裝及驗證

- 已安裝 Visual Studio Build Tools 2026 18.10.1、MSVC x64／x86（編譯器 19.51.36257.0）與 Windows SDK 10.0.26100.0，不需重新開機。
- MSVC x64 Release：完整專案建置通過；CTest 9／9 通過（3.87 秒）。
- MSVC x86 Release：完整專案建置通過；CTest 9／9 通過（4.44 秒）。
- 兩個架構都啟用 /W4 /WX，公開標頭檢查、字庫與真實資料查字流程皆通過。
- 獨立 SDK probe 的 x64／x86 版本皆成功編譯、連結並執行：TSF interfaces、COM imports、GUID、resource compiler 驗證通過。未註冊輸入法或操作宿主文字。
- 可直接用中文專案路徑。自動化環境須去除子程序的 Path／PATH 重複項，並使用單節點 MSBuild；重現步驟見 toolchain.md。
- 本次未驗證 MSVC Debug，也尚未實作 TSF 輸入法本體。以下「尚未驗證 MSVC／x86」是先前階段的歷史狀態。

## 2026-09-17：查字核心完成後

- GCC 16.1.0／CMake 4.3.2：Debug、Release 建置通過；警告視為錯誤，11 個公開標頭獨立編譯通過。
- Release 全部 9／9 CTest 通過（3.47 秒）。Debug 原全套除真實資料 console 輸出編碼比對外均通過；修正 CMake 測試端明確以 UTF-8 讀取後，該測試重新執行通過（2.49 秒）。未因純測試解碼修正重跑已通過的其他項目。
- engine.behavior 驗證命令、去重、排序、分頁、空候選、失敗查詢保留狀態、設定生命週期、128 筆上限、補充平面字、不同 context 狀態隔離及過期回呼。
- engine.real_data 從 Conway 原始資料建置索引，輸入「臺」完整筆劃，模擬提交失敗後再次選字成功；沒有輸入到 Windows 應用程式。
- 個人學習、台灣字頻、MSVC／x86、TSF 與候選視窗尚未驗證或實作。

## 2026-09-17：字庫整理完成後

- GCC 16.1.0／CMake 4.3.2：Debug、Release 建置通過，警告視為錯誤。
- Debug 7／7 CTest 通過（12.69 秒）；Release 7／7 通過（2.44 秒）。時間為本機單次測試耗時，不是逐鍵查字效能。
- 11 個公開標頭可獨立編譯，包含工具端 compiler.hpp。
- 全量 Conway 28,165 字、63,006 組展開配對與上游參考表完全一致。
- traditional 保留 25,611 字、59,640 配對；all 保留全部。測試明確涵蓋「国」未標記仍保留的分類限制。
- 155 種一至三筆前綴查詢與獨立線性比對結果一致。
- 補充平面字、反向引用、多筆順去重、覆寫衝突與層順序、未知版本、截斷／損壞／非法結構索引都有測試。
- CLI 測試驗證中文目錄、兩次建置 SHA256 一致、已存在字庫不可覆寫、錯誤補字檔不發布產物、來源與授權可追溯。
- 實際以完整中文絕對路徑產出 data/generated 中兩個資料包，查詢 `12125145154121` 得到「臺」，exact。
- traditional 索引 SHA256：`260FD8119F85E8FB49A4213E2666B0B142F84DDA33E78AA6DA317F2F682D9620`。
- all 索引 SHA256：`21157662719A1DD0E3DFF161101047E42CE568AC764799B77BF02261F7D4F677`。
- MSVC、x86 與 Windows TSF 尚未驗證。本階段沒有開發輸入法 UI 或註冊系統輸入法。

## 2026-09-16：原始框架紀錄（歷史）

日期：2026-09-16；本機 Windows x64。

| 項目 | 結果 |
|---|---|
| CMake 4.3.2／GCC 16.1.0 Debug configure、build | 通過 |
| CMake 4.3.2／GCC 16.1.0 Release configure、build | 通過 |
| Debug CTest | 4／4 通過 |
| Release CTest | 4／4 通過 |
| 公開標頭獨立編譯 | 8／8，Debug 與 Release 通過 |
| 編譯警告 | 啟用 -Wall -Wextra -Wpedantic -Werror，建置通過 |
| MSVC x64／x86 presets | 已提供，工具未安裝，未實測 |
| TSF、Windows 視窗、字庫轉換 | 未實作，無相關通過宣稱 |

CTest 包含框架契約、工具版本、工具說明、拒絕未實作操作。契約測試涵蓋 Unicode 補充平面／非法碼點、按鍵別名／衝突、非法筆劃／設定版本／頁大小，以及 Session 快照隔離與 context revision。Release 不以 NDEBUG 關閉驗證。

原中文路徑下的 MinGW Makefiles 建置失敗，提示來源與 DependInfo 路徑找不到；檔案實際存在。透過 V: 暫時映射後相同程式碼建置與測試成功。實測目錄：out/build/mingw-debug-ascii、out/build/mingw-release-ascii；映射於工作完成時移除。重建方式見 README。

沒有安裝新編譯器、沒有註冊系統輸入法、沒有執行研究用 Python，也沒有封裝 Conway 原始資料。這份紀錄是框架驗證，不是可用輸入法的驗收。

## WinUI 3 設定 App（2026-09-20）

- Windows App SDK WinUI 1.8.260803003 / C++/WinRT 2.0.250303.1，v145 Release x64 建置成功；完整依賴版本與雜湊固定在 apps/settings/winui/packages.lock.json。
- apps/settings/winui/test.ps1 通過：實際建立 XAML 視窗、標準／傳統預設、自訂重複鍵拒絕、筆劃預覽、反轉旗標儲存與重載、停用學習、深／淺 ActualTheme 切換；使用獨立資料目錄。
- 使用 computer-use 實際查看深／淺視窗、捲動內容及清除確認對話框；在試打區按 A 得到「一」，自訂橫鍵按 Z 得到 Z。清除對話框按取消，未刪正式紀錄。
- x64／x86 現有 18 項 CTest 各全數通過；CMake 兩架構重新 configure 成功，已移除舊 Win32 設定 target，新增獨立 WinUI target。
- 新 UI 沿用原有 storage C++ 模組；未新增或遷移正式使用者資料格式。新版未安裝，正式安裝包未產生。
- 尚未做乾淨機器部署、Windows 10 1809、高對比、螢幕閱讀器、多螢幕 DPI 切換與 OS 主題即時切換的人工驗收。程式使用標準 WinUI ThemeResource 與 ActualThemeChanged；已測程式內深淺色切換，不能等同全部 OS 情境已實測。

## 0.4.0 本機升級（2026-09-20）

- 已建置 StrokeIME-Setup-0.4.0-win64.exe 與對應 ZIP／SHA256；x64、x86 各 18 項測試通過，打包目錄的 WinUI 設定程式隔離測試通過。
- 依使用者要求安裝到本機，安裝程式 exit 0；兩架構 selftest／register 全部 exit 0，登錄 DLL 路徑指向 versions/0.4.0。
- 安裝後設定 EXE 及兩架構 DLL 的 SHA256 與 payload 相符；正式 layout.dat、learning.dat 安裝前後雜湊相同。
- Program Files 中的設定程式隔離功能測試通過；安裝紀錄 out/install-0.4.0.log。
- 建置時 NuGet 套件已成功還原，但弱點查詢因網路限制回報 NU1900；未把這次建置當成線上弱點稽核通過。

## 0.4.1 系統匣識別圖示修正（2026-09-20）

補上 GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT 的註冊與解除註冊，修正 Windows 用語言縮寫「繁體」取代品牌圖示的原因。兩架構各 18 項測試及打包後設定程式測試通過；本機安裝 exit 0，新增 category 確認存在，IconFile 指向 0.4.1。未操作遊戲、重啟工作列或強制切換輸入法，工作列最終顯示仍待使用者切換驗收。未新增第三方依賴。

## 0.4.2 候選框外觀（2026-09-21）

- 自繪候選框跟隨應用程式深淺色，高對比優先系統色，加入圓角區域／細框，移除底部操作教學。
- x64／x86 各 18 項測試通過，0.4.2 安裝 exit 0，安裝時兩架構 selftest／register 成功。安裝後 DLL 與 payload 雜湊相同。
- 未自動切換 OS 主題、操作遊戲或重啟使用者程式；深淺色切換與實際候選框外觀仍待人工驗收。UILess 宿主介面不受此自繪變更影響。

## 0.4.3 左右鍵翻頁（2026-09-21）

加入 ←／→ 對應 PageUp／PageDown。TSF 宿主測試驗證左右鍵、保留原翻頁鍵、非等長 UILess 頁面、idle 不攔截，以及 Shift／Ctrl／Alt／Win＋方向鍵不攔截。x64／x86 各 18 項測試通過，安裝 exit 0，兩架構 selftest／register 成功；安裝 DLL 與打包產物雜湊相同。
