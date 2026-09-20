# UILess 與延後提交（2026-09-20 開發版）

此改動尚未打包、安裝或註冊到使用者系統。已安裝的 0.3.1 不會自動取得此功能。新版安裝需執行兩架構的 register，才會加入 UIELEMENTENABLED 類別；只複製 DLL 不足以更新類別。

## 候選介面

- ActivateEx 不再拒絕 TF_TMAE_UIELEMENTENABLEDONLY；透過 ITfUIElementMgr 協商所有候選窗的顯示。
- CandidateElement 實作 ITfCandidateListUIElement，提供完整排序後候選清單、文件、選取索引、分頁與變更旗標。候選文字為 UTF-16 BSTR。
- 組字開始 BeginUIElement、內容／分頁變更 UpdateUIElement、取消／提交完成／焦點切換／停用 EndUIElement。
- 宿主回傳 pbShow=FALSE 或呼叫 Show(FALSE)，本輸入法不顯示自己的候選窗；候選物件仍可查詢。允許顯示時沿用現有 Win32 候選窗。
- UILess 宿主可用 SetPageIndex 指定合法分頁；引擎依同一分頁處理 PgUp／PgDn、數字與首字選取。原生候選窗使用九個一頁。宿主分頁超過九字時數字鍵只選該頁前九字，空白／Enter 仍選首字。
- 宿主即使保留已結束的候選 COM 物件，也不能透過它改動新組字；結束時移除回呼並解除文件參照。
- 尚未實作 ITfCandidateListUIElementBehavior（宿主以方法直接選字／Finalize），也未提供整合搜尋建議協定。此階段由輸入法按鍵路徑選字，不能宣稱所有遊戲皆已驗收。

## 提交生命週期

- 正常按鍵提交先請求 TF_ES_SYNC | TF_ES_READWRITE。
- 只有 RequestEditSession 回報 TF_E_SYNCHRONOUS、且 DoEditSession 尚未執行，才改請求 TF_ES_ASYNC | TF_ES_READWRITE。其他寫入錯誤不盲目重送。
- TF_S_ASYNC 只代表已排程，不視為成功，也不計入學習。DoEditSession 實際 InsertTextAtSelection 成功後才完成引擎提交、增加學習次數。
- WriteSession 防止重複回呼再次插字；插字成功後若游標移動失敗，也不能重試文字。
- 排隊的工作持有服務 COM 參照及 generation。取消、焦點/context 切換、外部文字／選取區修改、Deactivate 都使舊 generation 失效。
- 執行前重新確認目前焦點文件的 top context、唯讀與停用 compartment；失效工作不寫入。
- 按键排隊最多 128 筆，保留當時 Shift／Caps Lock；沒有下一組筆畫的重複選字鍵不加入佇列。Esc 取消排隊工作，其他交給宿主的導覽／快捷鍵會取消未完成輸入。若宿主長時間完全不處理寫入而填滿佇列，超過上限的按鍵不再加入，避免無界記憶體增長。
- 寫入回呼中不再立即請求候選位置的讀取工作；用本服務的訊息視窗延後刷新與處理後續按鍵，timer 可補處理刷新。失敗時保留原候選供重試，清除等待的後續按鍵。
- 空的 edit session／只有屬性更新不取消提交；真正外部文字或選取區變更才取消。

## 測試

新增 windows.tsf_host：使用實際 service.cpp、真實 Conway 字库、隔離的設定／學習目錄，以及可控制的 COM 宿主。測試不註冊輸入法、不注入實體鍵盤，也不修改使用者的設定／學習資料。

涵蓋 UILess-only／一般啟用、Begin／Update／End、pbShow=FALSE、Show／IsShown 與重複顯示要求、Begin 內停用的重入、完整候選、合法／非法宿主分頁、過期 UI 物件、同步成功、同步拒絕轉非同步、延後執行、重複回呼、快速連續輸入、Esc、焦點通知、沒有通知的焦點改變、唯讀轉換、外部編輯、空編輯通知、插入失敗與重試、非同步請求遭拒、Shift 英文及 Caps Lock 快照、停用後的遲到回呼與成功後才學習。

此測試是 adapter 協定驗證，不是真實遊戲、搜尋框、瀏覽器或 Office 的端到端驗收；發布前仍須進行實際應用程式測試。

## 依據

- [Microsoft UILess Mode Overview](https://learn.microsoft.com/en-us/windows/win32/tsf/uiless-mode-overview)
- [ITfCandidateListUIElement](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nn-msctf-itfcandidatelistuielement)
- [SetPageIndex](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nf-msctf-itfcandidatelistuielement-setpageindex)
- [RequestEditSession](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nf-msctf-itfcontext-requesteditsession)
