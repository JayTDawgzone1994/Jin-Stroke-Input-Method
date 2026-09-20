# 第一階段：字庫整理

完成日期：2026-09-17。全程 C++20，離線建置，不依賴 Python。

## 已完成

- 解析 Conway 的 1～5 筆劃、替代分支、空分支與單位數反向引用，保留同一字的多組筆順。
- 在任何篩選與覆寫前，與上游 `sequence-characters.txt` 比對全部配對；不一致就停止。
- 全字集／繁體模式、獨立補字與修正層、版本化二進位索引。
- 索引載入器驗證格式、界線、排序、碼點、CRC32 與字的順位一致性；支援前綴查詢及候選去重。
- 保留來源 commit、原始資料、授權、套用的修正檔及統計報告；相同輸入產出相同 bytes。

## 本次產物

來源 commit：`d66ba5f5aa4cb6583883dfe8c14de553bb43616a`（Conway v2.0.2）。

| 產物 | 漢字 | 字與筆劃配對 | 不重複序列 | 索引 bytes |
|---|---:|---:|---:|---:|
| data/generated/conway-v2.0.2-traditional | 25,611 | 59,640 | 57,072 | 1,583,355 |
| data/generated/conway-v2.0.2-all | 28,165 | 63,006 | 60,227 | 1,660,662 |

數值對應目前無實質修正的 project.tsv；NOTICE 文字或修正內容變更後，資料版本與檔案大小可能改變。

每個資料包包含 `dictionary.sidx`、`NOTICE.txt`、`report.tsv` 與 `sources/`。使用／散布索引時保留整包來源與授權資訊，不只拿走 binary。NOTICE 亦嵌入索引，可由 inspect 讀取。

## 繁體篩選的精確含義

`traditional` 排除 Conway 明確標為 `*` 的簡體專用字，保留 `^` 與未標記字；`all` 保留全部。本次排除 2,554 字。

這是**上游分類的保守篩選，不是台灣正字白名單**。實查「国」未標為簡體專用，因此仍保留；「汉」標為簡體專用，會排除。「國」與繁簡共用的「一」都保留。不能宣稱產物完全沒有簡體常見寫法，也不能據此保證台灣標準筆順。

需要排除特定異體字時，可用獨立修正層將它重新分類為 simplified（只在 traditional 排除），或 remove（兩個模式都移除）。不要直接修改上游來源，也不要做整批簡轉繁字串轉換，否則筆劃會對應到錯誤字形。

本階段不匯入香港常用字排序。`frequency_score = 0` 表示未評分；查詢以碼點維持穩定輸出。獨立新酷音分數表會在 load 時覆蓋分數，詳見 frequency.md；個人學習尚未實作。

## 使用方式

先依根目錄 README 編譯 Release，以下在專案根目錄的 PowerShell 執行。執行工具可直接使用中文路徑，不需要建置時的 V: 映射。MinGW 版執行環境需可找到其 runtime DLL（目前 MSYS2 bin 已在 PATH），這不是正式分發的獨立安裝包。

```powershell
$dictTool = '.\out\build\mingw-release-ascii\tools\dict_builder\stroke_dict_builder.exe'
& $dictTool build --source '.\data\upstream\conway' --output '.\data\generated\my-traditional-v1' --scope traditional --overrides '.\data\overrides\project.tsv'
& $dictTool inspect --index '.\data\generated\my-traditional-v1\dictionary.sidx'
& $dictTool query --index '.\data\generated\my-traditional-v1\dictionary.sidx' --strokes 12125145154121
```

最後一行可查到「臺」。數字對應：1 橫／提、2 豎／豎鈎、3 撇、4 點／捺、5 折／彎。

輸出目錄必須不存在；工具先驗證全部資料，在同父目錄的 `.staging` 新目錄完成寫入與重讀檢查，再更名發布。已有版本不覆寫。若程式遭外部強制中止留下 staging，先檢查它再選新的輸出目錄；工具不擅自接管既有 staging。

## 補字與修正格式

UTF-8 TSV，每行五欄，**使用真正的 Tab**：

```text
action    U+codepoint    expression    class    reason/source
```

- `add`：新增原字庫不存在的碼點。
- `replace`：取代既有字的整組筆順與分類。要保留舊筆順，必須把它寫進新的表達式。
- `remove`：移除既有字；expression、class 都填 `-`。
- class 是 `shared`、`traditional`、`simplified`。
- reason/source 必填，記錄依據；`#` 開頭行為註解。同一層不允許重複修改同一字。

可重複傳入 `--overrides`，例如專案修正先、個人修正後；按命令列順序套用，後層可 replace 前層結果。整層發生衝突就失敗，不產出部分成功的字庫。補字目前是「修改檔案後重建索引」，尚未做執行期補字 UI。

範例格式見 `data/overrides/project.tsv`；該檔目前沒有啟用的修正。測試用的「𠮷」等補字僅測機制，不作為經查核的正式新增字。

解析上限：來源文字 64 MiB、修正檔各 1 MiB、每字 4,096 組展開組合、序列 128 筆、10 萬字、100 萬配對。不支援任意 regex，只接受 Conway 的有限語法；反向引用始終是一位數，例如 `\11` 表示第 1 組後接筆劃 1。

## 索引格式 2（唯一支援格式）

所有整數為 little-endian uint32，無 C++ struct padding，不含平台指標。x86／x64 使用相同規格；MSVC／x86 尚未實測。

| Header offset | 欄位 |
|---:|---|
| 0 | 8 bytes ASCII `STROKE01` |
| 8 | 格式版本 2 |
| 12 | 配對數 |
| 16 | metadata byte 長度 |
| 20 | payload CRC32（IEEE） |
| 24 | payload 起點 |

Metadata 依序包含 data_version、source_revision、NOTICE；每個字串都是 uint32 byte 長度＋UTF-8 bytes。後續每筆 record 為碼點、frequency_score、序列 byte 長度、ASCII 1～5 序列；依序列、碼點排序，禁止重複配對，尾端不可有額外資料。同一字的所有序列需有相同 frequency_score。

CRC32 用於損壞檢查與重建追蹤，不是安全簽章。SOURCE.txt 的 SHA256 是原始檔案保存紀錄，工具不以其作來源認證；工具另記實際輸入 bytes 的 CRC32 並保存快照。要更新來源，需同時更新固定版本、SOURCE.txt 和完整資料測試的預期值。

## 查詢與記憶體

目前先把驗證過的索引載入不可變的自有 records，二分搜尋前綴起點，只走訪匹配範圍。不同筆順命中同一字會合併；任一序列完全匹配則 exact_match=true。空前綴回傳空列表，非法筆劃回傳 Error。返回的候選擁有自己的資料。

本版沒有做 memory mapping，避免在資料格式初定時引入平台生命週期。1.51 MiB 是檔案大小，不是實際記憶體用量；需要時可換 storage backend 而保持 IDictionary。候選排序／翻頁、TSF 整合與逐鍵延遲仍屬後續工作。

格式 2 的 frequency_score 越大越優先，0 表示未評分，0xFFFFFFFF 是合法最高分。只讀寫格式 2；其他格式直接回報 unsupported_version，不轉換。安裝包必須一起更新程式與字庫，打包前及複製後均用新版 decoder 驗證字庫。
