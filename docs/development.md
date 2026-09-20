# 開發規範

## 語言與版面

C++20、UTF-8、四空白縮排，採根目錄 .editorconfig／.clang-format。型別 PascalCase；函式、變數、namespace snake_case；私有成員加尾端底線。公開標頭放各模組 include/stroke，不從其他模組的 src 引入檔案。

## 資源與錯誤

- 優先值型別、標準容器與 RAII；不以裸指標轉移所有權。需要動態所有權時優先 unique_ptr，只有真實共享生命週期才用 shared_ptr。
- 可預期的資料、設定或 I/O 失敗回傳 Result／Status。不要用空候選假裝讀檔成功。
- 不在所有函式盲目加 noexcept。COM／WndProc 邊界未來必須攔截 C++ 例外並轉成平台錯誤，不能讓例外穿過系統回呼。
- 核心用 Unicode scalar，Windows 邊界才處理 UTF-16。不要把 wchar_t、Windows virtual-key code 或 HWND 當作核心資料型別。
- Session 限定由其擁有執行緒操作；非 thread-safe 設計不可由背景工作修改。

## 介面與測試

- 確實需要替換 backend 才使用抽象介面；內部邏輯不用為每個類別增加基底類。
- 每個模組透過 target_link_libraries 宣告依賴，禁止全域 include_directories 或手工串接另一 target 的產物路徑。
- 測試涵蓋外部可觀察契約和失敗路徑，不鏡射每個私有函式。現有測試不使用 assert，Release 仍會執行檢查。
- 公開標頭須可獨立編譯；新增標頭時加入 tests/CMakeLists.txt 清單。
- 本框架以編譯警告視為錯誤；目前只有自有程式碼。未來第三方依賴不強套此規則。
- 影響架構邊界的更動新增 docs/decisions 記錄原因、替代方案與影響。

## 範圍與版本管理

不在 configure／build／test 自動下載、註冊輸入法或修改系統設定。研究資料、產物、個人設定不混入正式來源。新增 Git 儲存庫前先檢查父目錄是否已管理此專案；Conway 下載目錄是另一個獨立 checkout，已在根 .gitignore 排除。
