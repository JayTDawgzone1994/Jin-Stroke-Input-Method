# 跨應用程式的個人資料

0.9.1 將偏好與學習資料放在 Windows Known Folder `FOLDERID_Profile` 下的 `.jin-stroke-ime`（一般是 `%USERPROFILE%\.jin-stroke-ime`）。設定程式、x64／x86 TSF 使用同一份 `layout.dat`、`learning.dat` 與學習鎖。

## 問題與位置選擇

MSIX 桌面宿主會將 AppData 檔案讀寫覆蓋到宿主的私有目錄。實測同一個 LocalAppData/StrokeIME/layout.dat，一般桌面程序讀到 SL4 傳統／逐字聯想設定；GPT 的執行環境則讀到 Packages/OpenAI.Codex 的 LocalCache 內舊 SL3 標準／連續設定。`GetFinalPathNameByHandle` 確認兩個實際檔案不同。

`KF_FLAG_NO_PACKAGE_REDIRECTION` 只控制 Known Folder 回傳的路徑，不能讓後續檔案存取忽略覆蓋。這台電腦連 LocalLow 也會覆蓋到宿主的 LocalCache，因此共用資料放在 AppData 之外。個人目錄保留每位使用者的隔離，不需管理員權限，不使用機器共用資料夾。

## 移轉

- 安裝程式在註冊新版 DLL 前，以原始桌面使用者執行 `stroke_setup migrate-storage`，避免先被宿主建立空白資料。`enable`、正式設定視窗及 TSF 啟動也執行可重複的移轉。
- 只在新檔不存在時複製原有 LocalAppData/StrokeIME 檔案；保留全部原始位元組、學習 generation、次數與手動詞。
- 透過來源 handle 檢查實際路徑。若來源是宿主私有覆蓋或其他不同位置，不用它初始化正式資料。設定程式與一般桌面／安裝程序會移轉原始檔。
- 暫存檔完整寫入並 flush 後，透過不覆蓋目的檔的 rename 完成移轉；其他程序先寫入的新資料優先。失敗保留原始資料並回報錯誤。
- 不刪除舊檔，不合併不同宿主的私人學習副本，以免把已清除或停用的舊世代資料復活。若曾在宿主中累積獨立資料，副本仍在原處，可另行備份。
- 升級後重開輸入法宿主與設定視窗。仍載入舊 DLL 的程序使用舊位置，不能與新版學習資料即時同步。

## 驗證範圍

`windows.storage_migration` 使用獨立測試資料夾，驗證完整移轉、原檔保留、重複移轉、清除後不復活、失敗保護、實際檔案路徑，以及 AppData 私有覆蓋不能污染新位置。另以原生背景程序與 MSIX 子程序比較共用設定讀取。

本次針對一般桌面及 MSIX 桌面宿主的檔案隔離，不宣稱所有 AppContainer 的權限已驗收，也不放寬個人資料夾 ACL。

參考：[Microsoft：Packaged desktop apps 的檔案系統行為](https://learn.microsoft.com/en-us/windows/msix/desktop/desktop-to-uwp-behind-the-scenes)、[Known Folder flags](https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/ne-shlobj_core-known_folder_flag)。
