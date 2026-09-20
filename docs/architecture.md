# 實際框架與依賴

此文件描述目前框架、字庫、查字核心與最小 TSF adapter；research/軟體架構評估.md 描述未來完整方案。

```text
stroke::domain               共用值型別與設定驗證（static）
    ↑
stroke::dictionary           不可變索引、Unicode、前綴查詢（static）
    ↑
stroke::engine               狀態機、固定排序、翻頁、提交請求（static）
    ↑
stroke::ui_contract          顯示快照介面（interface）
    ↑
stroke::tsf_contract         TSF context token／整合依賴（interface）
    └──→ stroke::storage_contract ──→ domain

stroke_dict_builder ──→ stroke::dictionary_build ──→ dictionary
stroke_engine_console ──→ engine（模擬宿主，不呼叫 TSF）
stroke_contract_tests ──→ tsf_contract（檢查共用契約，不是 Windows 測試）
```

Windows interface target 只傳遞 include 路徑與依賴。具體 `stroke_tsf` DLL 僅在 Windows + MSVC 建置，連結 engine 與系統 COM／視窗函式庫；`stroke_setup` 是獨立註冊與診斷工具。Conway 解析與覆寫在 dictionary_build，執行期不連結這些離線邏輯。整合限制見 windows-ime.md、決策 0004。

## 公開邊界

- `domain/include`：強型別 Stroke、char32_t Candidate、Command、Error／Result、Config。
- `dictionary/include`：IDictionary 只讀介面。回傳值擁有資料，不向 UI 暴露記憶體映射指標。資料去重屬 dictionary 契約，最終排序與翻頁屬 engine。
- `engine/include`：Session 每個輸入 context 一份，不可複製或移動；snapshot 是副本；reset 更新 revision。所有查字、排序、翻頁與提交狀態集中此處，具體契約見 engine.md。
- `windows/ui/include`：UI 顯示副本，只把帶 revision 的選字命令送回 controller；不可改 Session。
- `windows/storage/include`：IConfigStore 表達讀寫失敗。驗證／原子替換留給具體實作，不在按鍵回呼操作檔案。
- `windows/tsf/include`：ContextToken 同時包含 context lifetime id 與 revision，供未來拒絕過期 edit session。

## 接續實作原則

1. dictionary 的不可變索引已透過 IDictionary 注入 engine；詳細格式與限制見 dictionary.md。
2. engine 命令產生 Update／CommitRequest；只有 adapter 確認提交成功後才清空組字。目前未實作個人學習，要求開啟時回傳 unavailable，不假裝已啟用。
3. TSF controller 持有 Session 與 CandidateView；不能讓 TSF 反向滲入 domain 或 dictionary。
4. 新 Windows API 檔案只加入 Windows 具體 target，以 `WIN32`／所需 SDK 條件控制；目前跨平台可編譯的 contract 不需要假裝呼叫 Windows。
5. 新增實際 JSON、二進位格式與 Windows backend 時才選依賴；不為未實作功能先加入大型框架。
6. 資料格式版本、設定版本與軟體版本分開。目前 Config schema 1、索引只讀寫格式 2，不維護舊格式相容層。破壞格式相容性時需升級版本，不能靜默解讀舊資料。

設定介面已改為獨立的 x64 WinUI 3 / C++/WinRT 程式，沿用 storage 模組，與 TSF DLL 的 Win32 候選介面分開部署。決策見 [0007-winui-settings.md](decisions/0007-winui-settings.md)。
