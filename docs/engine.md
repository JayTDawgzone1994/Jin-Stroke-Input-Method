# 查字核心

完成日期：2026-09-17。模組為 `stroke::engine`，C++20 靜態函式庫，無 Windows API、視窗或檔案寫入依賴；個人學習透過設定快照與成功事件銜接。

## 已完成的流程

筆劃／設定按鍵 → 查字 → 去重與固定排序 → 候選分頁 → 選字請求 → 宿主回報成功或失敗。

- 逐筆增加、退格、取消，支援無匹配時繼續編輯與退格恢復。
- 支援查詢用 Stroke::wildcard（＊），每個代替恰好一筆；組字、退格與失敗恢復保留這個 token，選字仍提交漢字。
- 排序：完整碼匹配優先，再按較大 frequency_score，最後按 Unicode 碼點。目前透過獨立新酷音單字分數表排序，未評分為 0；詳見 frequency.md。
- 同一字的候選只顯示一次；重複候選的 exact_match 採 OR、score 取最大值，確保結果不受 backend 輸出順序影響。
- 顯示當頁候選、總候選數、頁碼、是否還有下一頁；新增／刪除筆劃後回第一頁。
- 不自動上字，即使只剩一字也必須選取；候選精簡、詞組不在此階段；個人學習見 learning.md。
- 支援補充平面字，選字文字使用 UTF-32，尚未在核心轉成 Windows UTF-16。

## Session 公開介面

| 方法 | 契約 |
|---|---|
| configure(dictionary, config, learned) | 只在 Idle 接受；保有字庫 shared_ptr 與設定副本。字庫不可為 null，設定不合法則保留原設定。learning_enabled 決定是否套用 learned 計數及發送成功選字事件 |
| snapshot() | 取得獨立副本，UI 修改副本不影響 Session |
| handles_key(key) | 只查設定，不查字、不改狀態，供未來按鍵測試回呼使用 |
| process_key(key) | 設定中的 ASCII 邏輯按鍵轉成筆劃；未綁定按鍵 consumed=false |
| process(command) | 處理 AppendStroke、Backspace、Cancel、ChangePage、SelectCandidate |
| complete_commit(revision, succeeded) | 驗證待提交版本，確認後才清空組字；失敗保留筆劃、頁碼與候選並更新版本 |
| reset() | 清空組字與候選，保留設定和字庫，更新 revision 使舊回呼失效 |

Session 由擁有者在單一執行緒操作，每個 context 一份；禁止複製／移動以免混淆生命週期。字庫／設定切換需先結束組字，再 configure。核心最多接受 128 筆，與索引序列上限一致。

## 命令及錯誤結果

- Idle 的退格、取消、翻頁：consumed=false，交給宿主。
- 組字中的翻頁到邊界：consumed=true，內容與 revision 不變。
- 查不到候選：不是錯誤，維持 Composing，可繼續打筆劃、退格或取消。
- 非法筆劃、過期／越界選字、超長輸入：回傳 Error，不改狀態。
- 字庫查詢失敗或回傳非法碼點：回傳 Error，保留查詢前的筆劃、候選與頁碼；刪掉最後一筆不需查字庫，仍能回 Idle。
- 所有 Result 中的正常錯誤都保留原狀；記憶體配置等 C++ 例外仍需由未來 COM／WndProc 邊界處理，不能穿過系統回呼。
- adapter 應先按命令／handles_key 決定按鍵是否屬於輸入法，再處理 Error；不能因查字失敗就把原本要攔截的字母洩漏到宿主。

## 選字與提交狀態

```text
Idle --輸入筆劃--> Composing --選字--> PendingCommit
  ↑                   ↑                    |
  |                   └------ 失敗確認 -----┤
  └-------------------------- 成功確認 -----┘
```

SelectCandidate 使用顯示快照的 revision 和該頁零起算 index。產生 CommitRequest 後進入 PendingCommit，尚未清空任何筆劃；宿主確認成功後才回 Idle。PendingCommit 除取消外的輸入命令會回報 unavailable，避免重複提交。

回報失敗時，回到 Composing 並保留原頁面，使用者可重新選取。重複、過期的成功／失敗回報都拒絕。

Cancel／reset 可使尚未執行的延後提交失效。未來 adapter 必須在實際寫入前檢查 ContextToken（context lifetime id＋revision），不能只在寫入後查版本。若文字早已寫入宿主，reset 不是撤銷宿主文字的功能；核心本身不操作宿主。

## 互動測試工具

建置後在專案根目錄執行：

```powershell
& '.\out\build\mingw-release-ascii\tools\engine_console\stroke_engine_console.exe' '.\data\generated\conway-v2.0.2-traditional\dictionary.sidx'
```

逐行輸入指令（每行 Enter）：

```text
12125145154121
pick 1
fail
pick 1
ok
quit
```

結果會顯示「臺」的 exact 候選、第一次模擬提交失敗後保留組字，第二次確認成功後顯示 `SIMULATED_COMMIT 臺` 並回 Idle。工具**只模擬宿主，不會把文字輸入記事本或其他軟體**。

其他指令：`back` 退格、`cancel` 取消、`next`／`prev` 翻頁、`show` 顯示、`pick N` 選當頁第 N 字（console 一起算，核心零起算）。一串數字會逐筆處理，中途錯誤時保留已成功處理的筆劃。console 輸出 UTF-8；終端機需使用相同編碼才能正確顯示漢字。

鍵位經 Config.bindings 注入，未綁死在查字邏輯。default_keyboard_config() 提供 QWE／ASD 與 UIO／JKL：上排點、折、萬用，下排橫、豎、撇。Windows virtual-key 與修飾鍵辨識在平台層。console 支援數字 1–5 與 ASCII `*`，例如 `1*34`。

## 驗證及後續

- `engine.behavior`：排序、去重、分頁、空候選、各命令、錯誤保留、128 筆邊界、Unicode、不同 Session 隔離、失敗提交及過期事件。
- `engine.real_data`：從固定 Conway 來源重建字庫，透過 console 實際查「臺」並測試失敗／成功提交。
- TSF adapter、候選視窗、新酷音字頻及個人學習均已接入；詳見 windows-ime.md、frequency.md、learning.md。
