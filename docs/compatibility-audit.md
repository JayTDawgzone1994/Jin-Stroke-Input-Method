# Windows 相容性盤點（2026-09-20）

後續進度：同日已在開發版補上 UILess 候選協定與同步遭拒後的非同步提交，詳見 [實作及驗證範圍](tsf-compatibility.md)。下表保留動工前的盤點；前兩項已不再是「完全未實作」，但真實應用程式驗收仍待進行。沒有安裝到系統。

本次是開發原始碼與微軟文件的靜態盤點，未重新實測各應用程式，也未修改或安裝輸入法。已安裝的 0.3.1 與目前開發版本不同：字頻、個人學習及反轉選字等後續改動尚未安裝。

## 結論與證據範圍

目前是具備基本 TSF 整合的可用輸入法，尚非完成廣泛相容性驗收的成熟產品。並非每一個 TSF 介面都必須實作，但有實際需要補足的協定與行為。不能保證所有 Windows 輸入欄位可用。

- 已有：ITfTextInputProcessorEx、ITfKeyEventSink、ITfThreadMgrEventSink、ITfTextEditSink、ITfEditSession，以及透過 ITfInsertAtSelection 提交文字。
- 已有：x64 / x86、TSF 註冊、IMMERSIVESUPPORT 類別、候選視窗 owner 與 IME 顯示事件；會尊重停用鍵盤與唯讀 context。
- 先前由使用者確認 Windows 搜尋框可用 A、1 顯示候選並輸出「一」。這是特定情境的成功案例。
- 最近 x64 / x86 各 17 個 CTest 通過是核心與自動測試結果，不是 17 種真實應用程式驗收。setup 自我測試主要驗證 COM 載入、介面及生命週期，沒有完整文字輸入流程。

## 缺口與優先序

| 優先 | 證據與缺口 | 影響及下一步 |
|---|---|---|
| 高 | service.cpp 的 ActivateEx 對 TF_TMAE_UIELEMENTENABLEDONLY 直接回傳 E_NOTIMPL；沒有候選 UIElement 協定 | 明確不支援 UILess-only 宿主。實作 ITfCandidateListUIElement 與 ITfUIElementMgr 的 Begin / Update / End、Show 控制，再註冊 UIELEMENTENABLED 類別；不能只加類別宣告。 |
| 高 | 提交文字只請求 TF_ES_SYNC \| TF_ES_READWRITE | 同步編輯在按鍵回呼內是合法設計，但宿主拒絕時目前沒有非同步路徑。補提交生命週期、焦點/context 過期檢查、完成後才學習，以及可控的非同步處理；不可盲目重試造成重複輸入。尚未實測找到特定失敗宿主。 |
| 高 | 個人設定與學習使用一般 LOCALAPPDATA 檔案與檔案鎖 | 一般桌面跨程序測試不能代表 AppContainer。需測試權限、資料可見性及失敗時的降級，評估符合沙箱安全模型的共享方式；可能是設定或學習失效，不一定是完全不能打字。唯讀字庫已放 Program Files。 |
| 高 | 沒有依 InputScope 或安全／隱私情境明確停止學習的處理 | 增加密碼、私人欄位及相關啟動旗標的學習抑制。這是防護缺口，並非已證實有密碼被儲存；部分宿主本身會停用 IME。 |
| 中 | 沒有 ITfContextComposition / ITfCompositionSink 與文件內組字；組字僅在自己的候選窗 | 應用程式只在提交後看到文字。設計正式組字範圍、取消、外部終止、焦點變更及選取區處理；是否顯示筆畫於文件內需另外決定，不應把所有目前成功輸入都判為錯誤。 |
| 中 | 沒有 ITfTextLayoutSink；候選窗固定像素尺寸，無完整每螢幕 DPI 處理 | 捲動、移動、縮放、換螢幕或觸控鍵盤彈出時可能位置不準／遮擋。補版面通知、DPI 與輸入面板避讓。設定視窗已有部分 DPI 處理，不能混為一談。 |
| 中 | 未接系統中英模式 compartment、語言列模式按鈕、標準功能提供者 | 補模式狀態同步、圖示與一致的設定入口。現有 Shift 暫時英文不等同系統中英模式。 |
| 中 | 候選窗僅繪字，無滑鼠選字、完整無障礙候選物件；未處理數字小鍵盤選字 | 加候選操作、讀屏與鍵盤導覽測試；觸控操作另行設計。已有 IME WinEvent 不等同完整無障礙支援。 |
| 中 | 每 500ms 在宿主執行緒進行設定／學習檔案同步 | 即使鎖不等待，磁碟 I/O 與紀錄解析仍可能影響流暢度。量測逐鍵耗時與大量學習紀錄，再改通知、快取或適當背景工作；目前沒有卡頓基準證據。 |
| 發布 | 無原生 ARM64 產物、簽章與廣泛升級驗收；安裝版本仍 0.3.1 | 下一包須更新版本並驗收乾淨安裝、直接更新、DLL 被占用、移除、不同使用者。ARM64 屬新增平台支援，不能宣稱現有包已支援。 |

搜尋建議整合 ITfFnSearchCandidateProvider / ITfIntegratableCandidateListUIElement 尚未實作。這些支援更完整的搜尋體驗；微軟文件明確容許未完整整合的 IME 在組字完成後提供文字，因此不能直接把它們當成先前搜尋框失效的原因。

詞組、聯想、重新轉換、自訂詞庫、設定匯出入等是可選產品功能，不是基本相容性的先決條件。優先補相容性，再依實際使用需求決定。

## 建議驗收範圍

先建立可控制的 TSF 測試宿主，覆蓋 UILess、同步拒絕／非同步完成、外部終止、context 切換、唯讀及失敗提交；再做真實應用程式測試：

- Windows 搜尋、設定搜尋、記事本。
- Edge / Chrome 的 input、textarea、contenteditable；Office 的 Word / Excel（可用時）。
- Electron 編輯器／聊天軟體、x86 Win32 程式；有需求時再納入特定遊戲、終端機與遠端桌面。
- 每項涵蓋組字、選字、取消、退格、翻頁、Shift 英文、反轉選字、切換輸入法／焦點，以及成功後學習一次、失敗不學習。
- 額外測試多螢幕與縮放、候選窗靠邊、觸控鍵盤、受限制宿主、權限不同的應用程式。安全桌面與密碼欄位不以「強制能輸入中文」為通過條件。

先做 UILess 與安全提交生命週期，再做共享設定／學習隔離及候選定位。每項分別留下可重現步驟與結果，不以單一搜尋框測通替代整體驗收。

## 參考

- [Microsoft：Custom IME requirements](https://learn.microsoft.com/en-us/windows/apps/develop/input/input-method-editor-requirements)
- [Microsoft：UILess Mode Overview](https://learn.microsoft.com/en-us/windows/win32/tsf/uiless-mode-overview)
- [Microsoft：ITfContext::RequestEditSession](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nf-msctf-itfcontext-requesteditsession)
- 原始碼：src/windows/tsf/service.cpp、registration.cpp、src/windows/ui/candidate_window.cpp、src/windows/storage/learning_store.cpp、layout_store.cpp、apps/setup/main.cpp、packaging/StrokeIME.iss。
