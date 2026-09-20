# 字庫測試資料（後續）

小型合成 fixtures 目前位於 unit/dictionary.cpp 與 integration/dictionary_cli.cmake，驗證展開、篩選、覆寫與索引，不作為語言學依據，也不併入正式資料。整份資料測試直接讀取 data/upstream/conway，對照上游 sequence-characters.txt 並檢查 155 種一至三筆前綴。CLI 測試包含可重現產物、中文路徑與拒絕覆寫既有產物。
