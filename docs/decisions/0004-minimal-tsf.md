# 0004：最小桌面 TSF 與同步提交

狀態：採用，2026-09-17。

為先完成記事本可用的整合，加入 MSVC 專用 TSF DLL 與 C++ 註冊工具。使用 SDK 的 WRL ComPtr 管理 COM，無 ATL／第三方框架依賴，共用 core 保持可移植。

每個 service 僅保留目前 context 的 Session，切換時取消並解除 sink，不保留跨 context 組字。context 的外部 edit 也取消組字。候選視窗不取焦點，以數字與翻頁鍵操作。

採同步 edit session，沒有 asynchronous fallback；拒絕寫入時保留候選供重試，因此本版無需延遲寫入佇列。將來若支援非同步，必須同時驗證 context generation、焦點與 revision，不能只比較 revision。

本版不在文件內建立預編輯 composition，不聲稱支援 UI-less／immersive。新增 composition range、display attributes、ITfUIElement 等能力時再擴充，並以各宿主實測驗證宣告。

部署與建置目錄分開，各架構分開存放，保留完整字庫授權包。system registration 顯式呼叫 setup，CTest 不修改系統 profile。正式 Program Files 安裝、升級交易與簽章另做。
