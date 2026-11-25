# Shift 中英文模式與混合組字（開發中）

2026-11-23 已完成 MSVC Release x64／x86 編譯與各 24 項自動測試，另補測模式按鈕與 conversion compartment 的雙向同步。0.10.0 已打包並安裝本機，以原生 TSF RichEdit 驗證實際輸入；使用者分別確認系統匣「中 劃／英 劃」。其他宿主、主題與縮放仍需逐項驗證。先前的 0.9.1 安裝包不包含本功能。

## 操作規則

- 單獨按下並放開左／右 Shift，切換筆畫與英文模式。放開才切換，長按重複訊號不重複切換。
- Shift 按住期間用過其他鍵，就視為組合操作，不再切換常駐模式。Shift＋方向鍵、Ctrl／Alt／Win 快捷鍵交給原程式；重疊按兩個 Shift 不切換。
- 筆畫模式按住 Shift＋A–Z 保留暫時英文，大小寫仍只依 Caps Lock；放開後繼續筆畫模式。
- 常駐英文模式使用一般英文大小寫規則：Caps Lock 與 Shift 共同決定字母大小寫。可輸入數字、空白與一般鍵盤的 ASCII 標點。
- 切換模式時清除尚未接受的筆畫，關閉候選；保留已暫選的中文與英文草稿，不自動選第一候選、不提前送出整段。

## 英文與草稿

沒有連續組字草稿時，英文直接輸出，不建立底線。只有尚未接受的筆畫，也不算已有中文草稿；暫時英文會清除這些筆畫並直接輸出字母。

已存在草稿時，英文插入目前游標位置，與中文共用同一段組字底線。即使目前為英文模式，仍可用左右鍵回到中文，再按下鍵重新選字；英文可移動、刪除，但沒有候選字。數字在中文候選窗開啟時仍選候選，關閉時在英文模式輸入原樣數字。Enter 在候選窗中接受字，候選關閉時送出整段。

英文作為原文保存，沒有筆畫查詢，也不參與詞庫選詞。新增英文不改動既有中文草稿。中文詞庫預選以英文分隔為不同區段；最終提交的英文不記入單字學習，也不將兩側中文串成一個學習詞。

## 右下角與狀態同步

固定識別圖示仍是「劃」；新增標準 `GUID_LBI_INPUTMODE` 模式按鈕，筆畫模式顯示「中」、英文模式顯示「英」。可點模式按鈕切換。

「中／英」使用透明背景的 32-bit 圖示，由輸入法直接繪製顏色，避免部分 Windows 11 工作列未套用 `TF_LBI_STYLE_TEXTCOLORICON` 的主題換色。依工作列使用的 Windows 模式（`SystemUsesLightTheme`）選擇深色白字、淺色黑字；Windows 模式與應用程式模式分開設定時，跟隨前者。高對比模式優先採用系統文字色。透過不顯示、不取得焦點的頂層視窗接收 `WM_SETTINGCHANGE`、`WM_THEMECHANGED`、`WM_SYSCOLORCHANGE`，通知 shell 重新取得圖示；每次 `GetIcon` 也重新讀取目前顏色。停用服務時一併銷毀監聽視窗。

`ITfLangBarItemButton` 與 Shift 共用同一個模式切換入口。透過 thread manager 的 `GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION` 讀寫 NATIVE 位元，接收外部狀態變更並通知圖示更新。保留其他轉換旗標，不以關閉整個輸入法的方式停用組字編輯。暫時英文不改變常駐模式圖示。

模式屬於啟用中的 TSF 執行緒，不改寫 layout.dat。啟用時採用 Windows 已提供的模式，未提供時採筆畫模式。焦點變更取消尚未完成的 Shift 手勢，保留模式；停用時解除按鈕與狀態監聽。若宿主未提供相關模式介面，保留鍵盤輸入功能。

## 維護分工

- `keyboard_policy.hpp`：隔離 Shift 手勢、暫時英文與常駐英文大小寫。
- `ContinuousSession`：以 `DraftCharacter::literal` 區分中文與英文；新增原文插入與只取消當前查詢的命令，沿用游標、刪除與交易提交。
- `service.cpp`：英文文字翻譯、TSF 組字更新與模式狀態。模式切換與按鍵使用同一條有界非同步佇列，維持前後順序；焦點失效使舊文字回呼失效。
- `input_mode_button.hpp`：系統模式圖示、點擊與 shell 更新通知。停用後移除 callback，避免 shell 保留的按鈕回呼已釋放服務。
- 已通過兩架構的 `windows.keyboard_policy`、`engine.continuous`、`engine.phrases`、`windows.tsf_host`、`windows.input_mode` 等完整測試。TSF 模擬宿主另驗證按鈕點擊、外部模式變更、其他 conversion 旗標保留、非同步順序、取消監聽與保留按鈕的生命週期。
- 安裝後實際試打涵蓋傳統／反轉選字、左右 Shift 切換、無草稿英文直接輸出、混合底線、清除未完成筆畫、英文無候選、英文模式回頭改選中文、Enter 結束底線、暫時英文保留草稿及 Shift＋方向鍵選取。測試時暫停學習，結束後以雜湊確認設定與學習資料完整還原。
- 測試窗須用 `EM_SETEDITSTYLE/SES_USECTF` 啟用 RichEdit 原生 TSF；預設相容路徑不能視為文件內組字驗收。其他應用程式、系統匣按鈕的實際滑鼠點擊、不同主題／DPI 仍待各環境測試。

參考：[Microsoft IME 系統匣要求](https://learn.microsoft.com/en-us/windows/apps/develop/input/input-method-editor-requirements)、[模式按鈕介面](https://learn.microsoft.com/en-us/windows/win32/api/ctfutb/nn-ctfutb-itflangbaritembutton)、[轉換模式旗標](https://learn.microsoft.com/en-us/windows/win32/tsf/flags-for-conversion-mode)。
