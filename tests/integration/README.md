# Windows 整合測試（後續）

Windows 整合需 MSVC 與 Windows SDK，以及真正的 TSF 實作，未來涵蓋焦點切換、延後 edit session、啟停／卸載、x86／x64 宿主及安裝回復。

目前 dictionary_cli.cmake 已測試 C++ 字庫工具的完整轉換／查詢、中文輸出路徑、可重現產物、補字來源追蹤與拒絕覆寫。這些是字庫 CLI 整合測試，不是 TSF 相容性測試。
