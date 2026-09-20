# Windows 最小 TSF 輸入法

這是可註冊、可切換、可在桌面記事本選字輸出的開發版。程式本體是 C++20，TSF 與 Win32 視窗都在 Windows adapter，核心不依賴 Windows。

## 使用

0.3.0 可從開始功能表的「錦筆劃輸入法 → 錦筆畫輸入法設定」選標準／傳統／自訂。以下鍵位與例子以標準為準；詳見 [配置與維護說明](settings.md)。

在 Windows 輸入法清單選擇「錦筆劃輸入法」，屬於中文（繁體，台灣）。可點工作列輸入法圖示切換，或使用 Windows 內建的 Win+Space。

| 筆劃 | 左手 | 右手 |
|---|---|---|
| 橫／提 一 | A | J |
| 豎／豎鈎 丨 | S | K |
| 撇 丿 | D | L |
| 點／捺 丶 | Q | U |
| 折 フ | W | I |
| 萬用 ＊ | E | O |

兩組可混用，每頁 9 個候選；✓ 表示完整筆序相符，其餘是前綴相符。

萬用鍵 E／O 輸入 `＊`，每個只代替一筆未知筆劃。例如 `一＊丿丶` 可找到「木」，也可以繼續匹配筆序更長的字；不會把 `＊` 寫進文件。可從第一筆使用，也可連續使用。✓ 在萬用查詢時表示候選字的某個筆序長度與查詢相同，不代表未知筆劃已被使用者確認。

支援六個輸入鍵及萬用查詢；尚未加入 G6 的六碼、詞組或標點模式。

更新後已開啟的應用程式可能仍載入舊 DLL；請先儲存文件，再關閉並重新開啟程式。必要時重新登入 Windows；不必刪除旧版部署目錄。

- 1–9 選字；空白或 Enter 選第一個。
- PageUp／PageDown 翻頁；Backspace 退一筆；Esc 取消。
- 無候選時仍可退格修改。
- 按住左／右 Shift＋A–Z 暫時輸入英文：Caps Lock 關閉小寫、開啟大寫；放開 Shift 恢復筆劃。首個字母成功輸出才清除未完成的查詢，單按 Shift 不取消。輸出失敗保留查詢，按鍵仍由輸入法消耗，避免錯誤大小寫。數字／符號及 Shift＋方向鍵交給應用程式，Ctrl／Alt／Win 快捷鍵照常。
- A 或 J 再按 1 得「一」；KIJ（251）再按 2 得「口」。完整「臺」筆序為 12125145154121。

完整匹配優先，其餘使用固定排序，尚無字頻或學習，罕字可能排在常用字前面。

## 建置與部署

使用 MSVC x64／x86 presets，產物在 `out/build/msvc-{x64,x86}/src/windows/Release/`：DLL、`stroke_setup.exe` 與完整 traditional 字庫包 `dictionary/`。建置與 CTest 不自行註冊。

若 `data/generated/conway-v2.0.2-traditional/` 尚不存在，先依 dictionary.md 建置。部署須保留完整字庫包的 NOTICE、來源與授權。

目前本機已改用安裝包，實際註冊位置：

```text
C:\Program Files\StrokeIME\versions\0.2.1\x64\stroke_tsf.dll
C:\Program Files\StrokeIME\versions\0.2.1\x86\stroke_tsf.dll
```

每個架構目錄各有 setup 與 dictionary。不要搬動或清除此目錄；先解除註冊才可移除。升級應部署到另一版本目錄，避免覆寫正在載入的 DLL。

一般使用者請用 [安裝包](installer.md) 安裝與移除。以下為舊開發部署的手動管理方式，並非目前安裝包的移除指令。從專案根目錄執行；register／unregister 需要管理員 PowerShell，其餘在目前使用者的一般 PowerShell 執行。

```powershell
# 管理員：兩個架構都要註冊
& './out/install/dev-0.2.0/x64/stroke_setup.exe' register "$PWD/out/install/dev-0.2.0/x64/stroke_tsf.dll"
& './out/install/dev-0.2.0/x86/stroke_setup.exe' register "$PWD/out/install/dev-0.2.0/x86/stroke_tsf.dll"

# 一般使用者：加入清單，不設為預設
& './out/install/dev-0.2.0/x64/stroke_setup.exe' enable
& './out/install/dev-0.2.0/x64/stroke_setup.exe' status

# 僅在診斷程序內啟用，檢查真實前景按鍵接收器
& './out/install/dev-0.2.0/x64/stroke_setup.exe' verify-registered
& './out/install/dev-0.2.0/x86/stroke_setup.exe' verify-registered
```

`activate` 會切換目前工作階段，供互動測試使用；`verify-registered` 只切換測試程序，建立有焦點的 TSF context 後確認 GetForeground 的 CLSID，不取代記事本測試。`selftest DLL` 僅驗證 COM factory、介面、無效參數與卸載契約。

目前 verify-registered 需要先在 Windows 切換到本輸入法；其他語言啟用時，ActivateProfile 可能成功但沒有前景接收器。工具會提示切換後再試，不將這種結果視為驗證通過。

## 停用與移除

先切到其他輸入法並關閉使用本輸入法的程式。

```powershell
# 一般使用者：從自己的切換清單移除
& './out/install/dev-0.2.0/x64/stroke_setup.exe' disable

# 管理員：解除兩個架構註冊
& './out/install/dev-0.2.0/x64/stroke_setup.exe' unregister "$PWD/out/install/dev-0.2.0/x64/stroke_tsf.dll"
& './out/install/dev-0.2.0/x86/stroke_setup.exe' unregister "$PWD/out/install/dev-0.2.0/x86/stroke_tsf.dll"
```

只操作本專案 CLSID／profile，不清理其他輸入法。若註冊中途失敗，可修正後重試，或用 unregister 清理本專案註冊；目前尚無自動回滾交易。DLL 被占用時需等相關程式結束才移除檔案。

## 實作與限制

- `src/windows/tsf/service.cpp`：service、按鍵、焦點／context、提交排程。
- `src/windows/tsf/write_session.hpp`：一次性提交、同步遭拒時的非同步路徑。
- `src/windows/tsf/candidate_element.hpp`：UILess 候選 COM 介面。
- `src/windows/ui/candidate_window.*`：不取焦點的候選視窗、UTF-32→UTF-16。
- `src/windows/tsf/module.cpp`：DllMain 與 class factory；DllMain 不讀字庫、不建立 COM。
- `src/windows/tsf/registration.cpp`：COM 註冊及官方 TSF profile/category API。
- `apps/setup/main.cpp`：命令列註冊、移除、狀態與診斷。

開發版先請求同步 edit session；若宿主拒絕同步而尚未執行寫入，改為非同步排程。只有實際插入成功才完成提交與學習。切換焦點／context、外部文字／選取位置變動及停用會使過期工作失效。文字插入成功後，即使移動游標失敗也不重試插入。完整狀態與測試見 [UILess 與延後提交](tsf-compatibility.md)。

0.3.1 新增 GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT，候選視窗綁定輸入欄位的 owner HWND，並發出 IME_SHOW／HIDE／CHANGE 事件。字庫隨程式安裝在 Program Files。後續開發版已加入 UIELEMENTENABLED 類別與 UILess 候選協定，以及字頻／本地學習；需新版安裝與註冊才生效。尚無整合搜尋建議、文件內預編輯、滑鼠選字、語言列按鈕、簽章與自動更新。未保證所有 AppContainer、遊戲、密碼欄位或遠端桌面相容。視窗固定尺寸，尚待改善高 DPI、無障礙與主題一致性。

API 依據：[TSF 註冊](https://learn.microsoft.com/en-us/windows/win32/tsf/text-service-registration)、[RegisterProfile](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nf-msctf-itfinputprocessorprofilemgr-registerprofile)、[InstallLayoutOrTip](https://learn.microsoft.com/en-us/windows/win32/tsf/installlayoutortip)、[RequestEditSession](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nf-msctf-itfcontext-requesteditsession)。



## 候選框外觀（0.4.2）

- 候選框使用 Windows 應用程式深／淺色偏好；高對比模式優先使用系統視窗／文字色。首次顯示及 WM_SETTINGCHANGE／WM_THEMECHANGED／WM_SYSCOLORCHANGE 更新配色。
- 移除方形 WS_BORDER，改為 8 px 圓角視窗區域與細框；不使用 WinUI、layered window，也不變更 NOACTIVATE、owner、IME light-dismiss 事件或 UILess 協商。
- 移除 1–9、PgUp/PgDn、空白／Enter、Esc 操作教學，以及無候選時的退格提示。仍保留筆劃、候選編號、精確匹配標記和頁碼；快捷鍵功能不變。
- 此配色僅影響輸入法自繪候選框。UILess 宿主自行繪製的候選介面仍由宿主控制。
- Windows 色彩偏好來源：https://github.com/microsoft/microsoft-ui-xaml/blob/main/docs/design-notes/resources.md
- 圓角區域 ownership：https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowrgn

## 左右方向鍵翻頁（0.4.3）

正在組字時，←／→ 分別沿用 PageUp／PageDown 的上一頁／下一頁命令；使用相同的候選頁邊界，包含 UILess 宿主設定的非等長頁。沒有組字時不攔截方向鍵。Shift／Ctrl／Alt／Win＋方向鍵仍交給宿主；沒有新增候選框底部教學文字。
