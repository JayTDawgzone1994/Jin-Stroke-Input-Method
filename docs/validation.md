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

## develop：完整打字區配置（2026-09-21）

- 設定介面改為 61 鍵主打字區：26 個字母鍵提供筆畫選單，35 個保留鍵停用，不再區分左右手。
- 增加任意多字母對應同筆畫、取消綁定、修改預設自動轉自訂，以及切換模式保留自訂草稿。
- SL2 儲存每個字母的筆畫；讀取 SL1 保留既有設定，寫入僅使用 SL2。設定介面與 TSF DLL 需一起更新。
- x64／x86 Release 各 18 項 CTest 通過；WinUI 隔離測試通過，檢查鍵盤／選單、保留鍵、配置切換、儲存與試打。
- 已檢視 RenderTargetBitmap 產生的深色、淺色與 600 DIP 寬度快照；高對比仍沿用 WinUI ThemeResource，未切換使用者系統高對比設定實測。
- WinUI 建置有 NU1900 弱點清單連線警告；編譯成功。本次不安裝、不發布 Release。

## 連續輸入模式（開發中）

- x64、x86 Release 建置成功，各 19 項 CTest 全數通過。
- 新增核心組字測試與 TSF 模擬宿主組字生命週期測試，涵蓋空白分字、修改、取消、Unicode、非同步連打與結束失敗重試。
- WinUI 3 建置及隔離 smoke test 通過，連續模式旗標隨套用保存；既有 SL2 設定預設關閉此模式。
- 測試修正：修字後按 Enter 必須將游標移至整段句尾，避免下一段插入已提交字串中間。
- 尚未安裝新版 DLL 做真實應用程式驗證；發布前需完成記事本、Windows 搜尋框、瀏覽器與聊天軟體測試。

## 2026-10-01 連續模式本機驗證

- 安裝的 0.6.0 未啟用連續模式：LocalAppData 設定仍為 SL1。從設定視窗開啟並套用後寫入 SL3、旗標為 1。
- 原生 Windows RichEdit（Msftedit.dll）實測 `A 空白 A 空白`，兩個「一」保留於同一段有底線的組字區；左鍵、下鍵、2 將第二字換成「有」，整段仍保留底線；Enter 提交「一有」後底線消失。
- 此次以實際註冊的 TSF DLL 測試；沒有模擬文字元件或以鍵盤貼上代替筆畫輸入。記事本啟動未取得視窗，尚未新增該宿主實測；Windows 搜尋／瀏覽器／聊天軟體仍需另行驗證。
- 0.6.1 補上連續模式尚未套用提示及操作說明。新酷音僅作為 TSF 流程參考，未複製其原始碼。
- 0.6.1 已在本機安裝成功；x64／x86 自我測試與註冊成功，安裝檔案 hash 與建置相同。已安裝 WinUI smoke test 通過，實際設定連續模式旗標為 1。

## 2026-10-01：新酷音詞庫串接／0.7.0 本機測試包

- 現用 Conway 字庫版本 `conway-d66ba5f5aa4c-traditional-E9F5BBE4` 整理出 128,957 詞、1,501,020 bytes 的 pidx；兩架構打包後詞索引 SHA-256 相同，原始 CSV／provenance／LGPL／來源 ZIP 的 hash 與上游快照相同。
- 最終 MSVC Release x64、x86 各 **21 項 CTest 全數通過**。核心測試包含手動鎖定、重疊詞、刪字回退、失敗保留；TSF 模擬宿主驗證非同步詞預選、候選重開、手動改字與 Enter 提交；轉換器驗證最高分合併、缺字／長度過濾及重現性。
- 實際完整詞庫效能測量：6 候選 × 64 字 × 10 次共約 1～2 ms；64 候選 × 256 字 × 3 次共約 277～351 ms（本機 Release）。這是測試資料上的純排序時間，不包含宿主／視窗時間，也不保證其他電腦的速度。
- 已產生 StrokeIME-Setup-0.7.0-win64.exe、ZIP 與 SHA256。打包後兩架構 COM selftest HRESULT 0，詞庫讀取測試及 WinUI 隔離 smoke test 通過；converter-source.zip 已檢查包含詞庫轉換／排序原始碼與授權。
- NuGet 弱點清單仍有 NU1900 網路警告；建置成功。本輪沒有安裝 0.7.0、沒有推送 Git 或發布 Release。已安裝 0.6.1 的實際 RichEdit 組字驗證不能當成新詞庫的端到端驗收；新詞庫仍需安裝後實際試打。

## 2026-10-01：逐字聯想／0.8.0 本機開發版

- 已新增詞庫接續查詢、短上下文、聯想候選狀態、WinUI 開關及 SL4 儲存。資料仍使用既有 pidx，沒有新下載或引擎依賴。
- MSVC Release x64、x86 各 22 項 CTest 通過。TSF 模擬宿主驗證「你 → 好 → 世 → 界」每字直接輸出，沒有 composition；失敗重試、反轉、學習停用、外部編輯與失焦皆涵蓋。
- 聯想提交取得寫入鎖後會比對選取與原字尾位置。測試將非同步待提交的游標移動、但不發通知，仍拒絕插入；游標設定失敗時不開聯想，但成功插入的字不重試、不遺失。
- WinUI 隔離 smoke test 通過；已檢視 habits-dark.bmp／habits-light.bmp，聯想與連續開關、說明和模式停用顯示完整。模式切換保留聯想偏好，需套用後寫入 SL4。
- NuGet 弱點來源仍有 NU1900 網路警告，編譯成功。未安裝本輪 DLL、未推送或發布 Release；真實應用程式的聯想窗仍待安裝後試打，本輪模擬宿主不能取代實際驗收。

- 0.8.0 最終測試安裝包已產生。打包後兩架構 COM selftest HRESULT 0，詞庫測試與 WinUI smoke 通過；DLL／setup 與建置一致，converter-source.zip 內主要修改原始碼與目前內容相符，installer SHA256 及 ZIP 內容驗證通過。

## 2026-10-01：0.8.1 候選框縮小

- 以實際 DrawText 字型與換行規則量測寬高，取代固定 400 像素寬；移除可見品牌字，底部使用數字比例，保留現用 20 像素字型、主題與圓角。
- 使用 CandidateWindow 本身產生隔離預覽，已檢視正常九候選（92×291）、二字聯想（114×116）與長筆畫／補充平面字（300×116），沒有文字裁切。這些尺寸對應本機測試字型與資料，不保證其他 DPI 尺寸相同。
- 此次只改候選視窗呈現；預覽程式為 out 內的開發工具，沒有加入產品執行時依賴或修改使用者文件。

- 最終 x64／x86 各 22 項既有 CTest 通過；0.8.1 安裝包、ZIP 與 SHA256 已產生並驗證，打包 DLL 與建置一致，轉換來源包包含相同候選視窗程式。未安裝或 push。

## 2026-10-01：0.8.2 連續輸入顯示未確認筆畫

- 目前查詢不論是否有候選，組字區皆顯示一丨丿丶フ＊的筆畫順序；已接受的草稿字保持漢字，重開既有字候選不額外插入筆畫。組字游標按筆畫數移動。
- MSVC Release x64／x86 各 22 項 CTest 通過。新增回歸檢查涵蓋有候選時仍顯示筆畫、多筆游標／退格、補充平面字前插入與萬用筆畫、開候選保留原筆畫、Enter 不提交筆畫，以及詞庫等候接受漢字後才選詞。
- 模擬 TSF 宿主驗證同步多筆組字與非同步「女丨 → 你好」更新；本輪未安裝新 DLL，真實應用程式仍待新版安裝後試打。
- 0.8.2 安裝包與 ZIP 已產生；NuGet 弱點來源有 NU1900 網路警告，建置成功。沒有 push 或發布 Release。

## 2026-10-01：0.8.3 連續／逐字聯想互斥

- 設定介面開啟連續模式時，關閉逐字聯想並停用該開關；關閉連續模式後，聯想仍保持關閉。SL4 讀寫都清除衝突的聯想旗標，鍵位與反轉設定不變。
- MSVC Release x64／x86 各 22 項 CTest 通過；新增設定互斥、既有衝突設定正規化及聯想模式保存檢查。TSF 模擬宿主允許原生 UI 時，成功送字後會顯示聯想候選，並驗證本程序的候選視窗可見；既有 UILess、非同步與選字測試繼續通過。
- WinUI 隔離 smoke test 驗證開關自動關閉、切回逐字保持關閉、重新啟用聯想可保存，以及深淺色呈現；已檢視 habits-dark.bmp／habits-light.bmp，說明完整換行、沒有裁切。
- 這次缺少聯想的本機設定仍為 SL3（連續模式啟用，聯想預設關閉）；0.8.2 已安裝的設定程式通過隔離保存測試，詞庫與開始功能表設定捷徑存在。尚未在實際 RichEdit 重現聯想故障，使用者選擇先重新登入再試；未修改其設定或再次重啟服務。
- 0.8.3 本機安裝包、ZIP 與 SHA256 已產生；payload DLL／設定程式與建置一致、對應原始碼 ZIP、installer hash 與散布 ZIP 內容通過驗證。NuGet 弱點來源仍有 NU1900 網路警告，建置成功。未安裝或 push。

## 2026-10-01：0.9.0 個人詞庫與聯想學習

- MSVC Release x64／x86 各 22 項 CTest 全數通過。新增相同前文接字排序、手動補詞、三次門檻、停用學習保留手動詞及最長前文優先測試。
- TSF 模擬宿主驗證普通模式成功輸入「你子」三次後，原固定詞庫缺少的「子」能在「你」後成為聯想首字並成功輸出；也驗證手動詞逐字串接、重開後排序保存、連續模式只學最終提交，以及未通知的游標移動中斷詞學習。
- 儲存測試驗證兩個獨立程序累加單字及詞語各 40 次、原子替換失敗重試、格式 1 單字紀錄遷移、移除／清除後舊批次無法復活、清除保留手動詞及非法詞條拒絕。
- WinUI 與打包後設定程式隔離 smoke test 通過：新增、搜尋事件、移除、重新整理、自動詞清單及補充平面漢字；已檢視深淺色及 600 DIP 寬度個人詞庫快照，清單與按鈕完整呈現。控制項使用 WinUI 主題資源，未切換使用者系統高對比設定實測。
- 0.9.0 安裝程式、散布 ZIP 及 SHA256 已產生。payload DLL／setup 與建置一致，兩架構 COM selftest HRESULT 0；對應來源 ZIP 含本輪核心／儲存／UI／測試程式及個人詞庫說明，installer hash 與散布 ZIP 內容驗證通過。
- NuGet 弱點清單仍有 NU1900 網路警告，編譯成功；未安裝、未 push 或發布 GitHub Release。個人詞目前只參與逐字聯想，連續模式可教詞但整段預選仍使用固定詞庫；真實宿主的個人聯想體感待安裝後試用。

## 2026-10-08：0.9.1 共用設定與 MSIX 私有覆蓋

- 找到使用者回報的設定不一致：設定 EXE 的原始 layout.dat 為 SL4 傳統配置、反轉選字、逐字聯想；GPT 執行環境則透過 AppData 覆蓋讀到舊 SL3 標準配置與連續模式。CreateFile 的最終 handle 路徑確認兩份檔案不同。LocalLow 也會被覆蓋，NO_PACKAGE_REDIRECTION 無法阻止檔案覆蓋。
- 傳統 QWQW 對應 1212，安裝的 0.9.0 字庫有 1725 個前綴候選；舊標準配置對應 4545，候選為 0。這解釋錯誤筆畫與空候選框，尚未自動操作 GPT 輸入欄位驗收。
- 已備份原始及宿主私有的 layout.dat、learning.dat；將此宿主的舊 layout 私有副本保留為改名備份。GPT 環境再次開啟舊正式路徑時，已讀到原始 SL4 傳統／逐字設定；進行中的組字要先取消，下次 idle 才生效。
- 新版改用 FOLDERID_Profile/.jin-stroke-ime。原生背景程序執行 migrate-storage 後，與 MSIX 子程序 --probe 讀到相同路徑及設定；原有桌面設定與學習檔案 SHA256 前後一致。私有學習副本獨立保留，不與不同 generation 資料任意合併。
- MSVC Release x64／x86 各 23 項 CTest 全數通過。新增測試包含保存鍵位後 QWQW 查字、整份資料移轉、來源保留、不覆蓋新資料、清除後不復活、目的檔錯誤與私有覆蓋不能初始化共用位置。
- 打包 WinUI 隔離 self-test 通過，涵蓋鍵位、預覽、模式互斥、逐字聯想、個人詞庫、學習與主題切換。
- 0.9.1 本機安裝包、ZIP 與 SHA256 已產生。尚未安裝新版 DLL，這台電腦目前仍載入 0.9.0；需要管理員提示由使用者核准，未在使用者休息時啟動安裝。未 commit、push 或發布 Release。

## 2026-11-22：Shift 中英文切換與混合草稿（尚未驗證）

- 依使用者實測新注音的操作確認規格：切換只清除未完成筆畫，保留可修改的中文草稿；已有草稿時英文共用底線但無候選，沒有草稿時直接輸出英文。
- 已修改 Shift 手勢、暫時／常駐英文、連續草稿原文字元、非同步模式佇列、學習分隔，以及標準中／英模式按鈕與 conversion compartment 同步。固定劃圖示保留。
- 已新增或擴充 keyboard、continuous、phrases、tsf_host、input_mode 的回歸測試程式碼，涵蓋組合鍵與重複探測、混合草稿重選、非同步輸入順序、普通逐字模式、英文學習排除及模式按鈕釋放後的安全性。
- 使用者正在打遊戲並明確要求先不要測試；本輪未編譯、未執行任何測試、未啟動視窗、未安裝、未 commit 或 push。上方歷史測試結果不適用於本輪修改。先前 0.9.1 安裝包未重建，不包含此功能。
- 下一步待使用者方便時編譯兩架構、執行回歸測試，再做實機輸入及圖示驗收。詳細行為見 input-mode.md。

## 2026-11-23：Shift 功能編譯與自動測試

- 使用者已授權測試。MSVC Release x64／x86 均成功編譯；首次編譯發現模式按鈕缺少 ctffunc.h 及 olectl.h，補上標頭後通過。
- x64、x86 完整 CTest 各 24 項通過，涵蓋 Shift 手勢、Caps Lock／常駐英文大小寫、混合草稿、中文重選、英文無候選、詞庫分隔、逐字模式及非同步輸入順序。
- 為避免僅測到缺少模式介面的降級路徑，新增 tsf_mode_fixture.hpp，提供隔離的 compartment 與 shell item manager；兩架構重新編譯並執行 windows.tsf_host，皆通過。驗證 Shift、按鈕點擊、外部模式通知、其他 conversion 旗標保留、模式與按鍵排隊、取消監聽及停用後保留按鈕不可回呼已釋放服務。
- 測試使用獨立資料，未修改正式按鍵設定與學習檔。建置需在沙箱外存取 MSBuild 共用元件，原沙箱程序已停止；沒有改系統註冊或安裝產品。
- 實際註冊仍指向 Program Files/StrokeIME/versions/0.9.0/x64/stroke_tsf.dll。尚未重建安裝包或在記事本、瀏覽器、系統匣做新版實機驗收；自動測試通過不代表這些宿主均已驗收。未 commit 或 push。

## 2026-11-23：0.10.0 打包、安裝與原生 TSF 實測

- 已產生 0.10.0 安裝程式、散布 ZIP 與 SHA256，打包流程兩架構各 24 項 CTest 及 WinUI 隔離自我測試通過。NuGet 弱點資料來源出現 NU1900 網路警告，套件還原與編譯成功。
- 安裝紀錄確認兩架構 selftest／register 均 exit 0，機器註冊指向 versions/0.10.0。已安裝 DLL／setup 與 payload 雜湊一致，安裝前後正式 layout.dat、learning.dat 逐位元相同；不需重開 Windows。
- 原測試窗未開啟 RichEdit 的 SES_USECTF，連續組字走到相容路徑而未在文件內顯示。依 Microsoft EM_GETEDITSTYLE 文件補上測試窗的 EM_SETEDITSTYLE，重新編譯測試窗；此處未改產品 DLL。新的實測窗已載入 versions/0.10.0/x64/stroke_tsf.dll。
- 實測傳統配置 Q 顯示橫畫，反轉選字按 9 輸出「一」。單按左 Shift 切英文後 Q 直接輸出 q；右 Shift 可切回筆畫，按住 Shift+A 輸出 a，放開後 Q 仍進入筆畫查詢。單按 Shift 會取消未接受筆畫，不自動送出候選。
- 連續模式實測「一丨」按 Shift 後保留帶底線的「一」；加入 abc 得「一abc」，英文位置按下鍵沒有候選。在英文模式往左回到「一」，按下鍵再按 8 改成「有abc」，英文與底線保留；Enter 後整段底線結束。
- 實測 Shift+Left 能選取 c，之後 Q 仍直接輸出 q，組合操作不誤切模式。中文草稿按住 Shift+A 得到同段帶底線的「一a」，放開後 Q 仍追加筆畫；取消最後筆畫再 Enter 正常送出。
- 工作列不在操作工具的可擷取視窗內；使用者分別回覆確認「英 劃」與「中 劃」。未宣稱已測滑鼠點擊系統匣、所有應用程式或所有主題／DPI。
- 測試僅用自行開啟的空白窗，不讀取既有文件。測試期間停用學習，結束後關閉測試窗與設定視窗，先確認沒有其他設定／詞庫修改，再原子還原備份；layout.dat、learning.dat SHA256 均與安裝前一致。原本連續模式關閉、逐字聯想開啟、傳統與反轉設定已保留。
- 未 commit、push 或發布 GitHub Release；已開啟的舊程序仍需重開才能載入新版 DLL。
