# 參與開發

歡迎以繁體中文或英文提交 Issue / Pull Request。

1. 先依 README 建置並執行測試。
2. 一個 PR 專注一個問題，說明使用情境、修改後行為及驗證結果。
3. 核心維持 C++20；平台相依程式放在 Windows 層。遵循 `.clang-format` 與 `.editorconfig`。
4. 改動查字、排序、按鍵或 TSF 行為時，增加能重現問題的測試。UI 另需檢查深淺色、高對比及縮放。
5. 不提交 `out/`、本機設定、學習紀錄、憑證或未獲授權的字庫。

筆畫修正放在 `data/overrides/project.tsv`，不要直接改上游快照；格式與重建方式見 `docs/dictionary.md`。引入依賴時更新 `THIRD_PARTY_NOTICES.md` 和 `note.md`。

提交自有程式碼即表示願意以本專案 MIT 授權提供該貢獻；第三方內容必須清楚標示來源與原授權。

回報相容性問題時，請提供 Windows 版本、應用程式名稱／版本、輸入法版本、鍵位配置、重現步驟與預期結果。截圖請遮蔽私人內容，不要上傳個人 `learning.dat`。
