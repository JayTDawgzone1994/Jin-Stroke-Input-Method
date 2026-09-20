# 新酷音單字分數

目前以獨立字頻表覆蓋 Conway 查詢結果的 frequency_score，沒有引入新酷音引擎。索引仍只接受格式 2。

## 資料與規則

- 上游快照：libchewing-data `c44e81aef24b06f1509f19e1be54c99812d0c43f`，原始檔與下載 SHA-256 見 `data/upstream/libchewing-data/provenance.json`。
- 26,073 筆單字符記錄，134,385 筆多字記錄。排除注音、聲調等非漢字後，產生 18,723 個漢字分數，輸出 131,325 bytes。
- 同字多讀音取最高分，不相加；例如行 34993、的 147270、一 34112。
- 繁體 Conway 25,611 字中，有 18,466 字對應資料，其中 14,250 字分數大於零；沒有對應的 7,145 字仍保留為 0 分。
- 分數是上游經過調整的輸入法優先值，不宣稱為一般語料出現次數。多字詞暫不參與排序。
- 最終排序：完整吻合優先、分數降冪、碼點升冪。同字多筆順只顯示一次。

## 建置與載入

純 C++20 轉換指令：

```text
stroke_dict_builder frequency --source data/upstream/libchewing-data --output NEW_DIRECTORY
```

轉換器針對此快照的三欄未加引號 CSV，遇到不符格式、負數或溢位即失敗，不猜測修復。輸出按 Unicode 排序，可重現；不覆寫已有目錄。將生成目錄的內容放入字庫旁的 `frequency/`。

`IndexedDictionary::load` 在啟動時讀取 `dictionary.sidx` 同目錄下的 `frequency/character-score.tsv`。格式為 UTF-8、LF，首行 `STROKE-FREQUENCY-1`，後續 `單字<TAB>uint32 分數`。單字不能重複。最大 4 MiB／10 萬項。

載入後字典不可變，不在按鍵回呼讀檔。表存在但損壞時明確失敗；表不存在則使用索引本身分數，便於基準比較。正式打包要求字頻表和授權來源齊全，因此不會無聲漏包。更新字频表後需重新啟動載入該輸入法的程序。

`decode` 僅解碼索引 bytes；檔案旁的字頻表只由 `load` 載入。Windows TSF、console 和字庫 inspect/query 工具均使用相同載入路徑。query 額外輸出分數欄。

## 來源與重新建置

`frequency/NOTICE.txt` 標明 LGPL-2.1-or-later、來源及修改；`frequency/sources/` 保留未修改上游檔、完整快照、授權和 provenance。Conway 原始 NOTICE 保留，兩份資料分開。

`frequency/converter-source.zip` 保存本專案轉換工具及其建置相依程式碼；打包時重新產生。解壓後可用 CMake 設定 `BUILD_TESTING=OFF`、`STROKE_BUILD_TOOLS=ON`，建置 `stroke_dict_builder` target，再以 `frequency/sources` 為輸入重新產生資料。Windows 使用支援 C++20 的 MSVC，來源路徑含中文時使用 UTF-8。

這次完成核心與資料整合，未產生新安裝包，也未更換系統中已安裝的輸入法。已加入本地個人學習，詳見 learning.md。

## 候選前後比較

以下由相同格式 2 索引，分別不載入／載入字頻表，透過實際 Session console 取得前 5 字；全部查詢候選數相同。1=橫、2=豎、3=撇、4=點、5=折、*=任一筆。

| 筆畫 | 原本前 5 字 | 新排序前 5 字 |
| --- | --- | --- |
| 1 | 一、㐁、㐂、㐄、㐉 | 一、有、地、或、下 |
| 12 | 丁、丅、十、㐁、㐉 | 十、丁、丅、地、或 |
| 1234 | 木、朩、㓼、㙬、㝳 | 木、朩、本、來、棋 |
| 251 | 卂、口、囗、㐕、㒭 | 口、卂、囗、是、中 |
| 4 | 丶、㐎、㐔、㐘、㐢 | 丶、之、試、所、請 |
| 1*34 | 仄、天、夫、戈、木 | 天、木、夫、仄、戈 |

完整吻合優先仍會讓「朩」「卂」等生僻字排在較長的常用字前面；這是目前保留的規則，尚未改成全體候選純分數排序。
