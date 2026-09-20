# 0005：單筆萬用查詢與雙手預設鍵位

採用：2026-09-17。

`Stroke::wildcard` 是查詢用 token，數值 6。`is_valid` 仍只接受實際筆劃 1–5；`is_query_stroke` 額外接受 wildcard。字庫檔案格式、版本、來源及授權不變，編碼器／解碼器仍拒絕把萬用字元存為字的筆序。

查詢使用第一個萬用符之前的固定前綴做 lower_bound，再在該前綴範圍逐筆比較。每個萬用符恰好比對一筆，不展開 5^N 種組合。最壞掃描範圍是現有索引一次，仍受 128 筆與索引載入大小限制；不截斷候選。沒有萬用符時沿用固定前綴範圍。

`exact_match` 指某個相符筆序與查詢長度相同，多筆序字去重時取 OR，分數取最大 frequency_score。Session 保留 query tokens，所以取消、退格、翻頁與提交失敗恢復不需要另建狀態機。

`default_keyboard_config()` 集中維護預設值：QWE／UIO 是點、折、萬用；ASD／JKL 是橫、豎、撇。Config 空 bindings 的舊契約保留，Windows adapter 顯式注入預設配置。沒有新增個人設定格式。
