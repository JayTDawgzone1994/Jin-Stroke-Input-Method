# 資料配置

- upstream/：固定版本的原始資料與來源／授權清單。
- overrides/：補字、筆順修正與原因，不修改 upstream。
- ranking/：台灣候選排序資料。

目前 upstream/conway 已固定到 d66ba5f5aa4cb6583883dfe8c14de553bb43616a，含原始資料、上游生成參考表、README、CHANGELOG 與 SOURCE.txt。產物位於 generated/（不納入版控），有繁體與全字集版本。詳見 ../docs/dictionary.md。

overrides/project.tsv 目前只有格式說明，尚無啟用的字形或筆順更動；補字測試在 tests 使用合成 fixture，不會混入正式索引。ranking 尚未提供台灣字頻，所有字的 base_rank 暫為未排序值。

Conway 核心筆劃資料為 Copyright 2021--2026 Conway，CC BY 4.0：https://creativecommons.org/licenses/by/4.0/ 。原始檔案保持不變，輸出包另附 NOTICE、來源與修正檔快照。建置使用 C++，不執行 Python 或連網。
