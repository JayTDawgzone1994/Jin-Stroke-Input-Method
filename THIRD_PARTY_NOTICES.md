# 第三方來源與授權

根目錄 MIT 授權適用於本專案自行撰寫的程式與文件，不取代以下資料、工具及執行階段的授權。第三方檔案內原有的聲明仍有效。

## Conway Stroke Data

- 作者：Copyright 2021–2026 Conway (@yawnoc)。
- 來源：https://github.com/stroke-input/stroke-input-data
- 版本：v2.0.2，commit `d66ba5f5aa4cb6583883dfe8c14de553bb43616a`。
- 筆畫資料授權：[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)。
- 保存於 `data/upstream/conway/`；來源與雜湊見 `SOURCE.txt`，各上游檔案的授權範圍見該目錄 README。
- 本專案的修改：展開替代筆畫、排除僅簡體字、去重、套用補字檔並建立查詢索引。繁體篩選保留繁簡共用字，不代表臺灣標準字形認證。
- 生成字庫附帶 `NOTICE.txt` 與原始資料；不得暗示原作者為本專案背書。

## libchewing-data

- 作者：libchewing Core Team；原始 CSV 的版權及授權聲明予以保留。
- 上游：https://codeberg.org/chewing/libchewing-data
- 固定快照：https://github.com/chewing/libchewing-data/tree/c44e81aef24b06f1509f19e1be54c99812d0c43f
- 授權：**LGPL-2.1-or-later**，見 CSV 檔頭及 `data/upstream/libchewing-data/COPYING.LGPL-2.1.txt`。
- 保存未修改的 CSV、完整 `source.zip` 及來源／SHA-256 紀錄 `provenance.json`。
- 本專案擷取單一漢字的分數，多讀音取最高分，產生獨立字頻表；衍生字頻資料依原 LGPL 授權提供。未連結新酷音引擎。
- 發布字頻表時保留其 NOTICE、LGPL 全文、可修改原始資料及對應轉換程式；安裝包的 `dictionary/frequency/` 包含這些資料與 `converter-source.zip`。

## Windows 設定介面與建置相依套件

設定介面使用 Microsoft Windows App SDK / WinUI 3、C++/WinRT 及 MSVC 執行階段。套件版本與內容雜湊固定於 `apps/settings/winui/packages.lock.json`。

- Windows App SDK 的 NuGet 二進位套件遵循各套件隨附的 Microsoft 軟體授權條款與第三方 NOTICE，不能一概視為本專案的 MIT 授權。
- C++/WinRT 工具／標頭依其套件 MIT 授權。
- WebView2 是建置相依套件；本程式沒有 WebView 畫面，也不安裝 WebView2 瀏覽器執行階段。
- MSVC app-local CRT 按其再散布條款提供。建置脚本將相應授權、Redist 與第三方聲明複製到設定程式的 `licenses/`。
- Windows SDK、MSBuild、CMake、Inno Setup 等建置工具各依原授權使用；不將它們的安裝檔納入此 Git 儲存庫。Inno Setup 編譯器的使用亦須遵循其授權條款。

重新散布安裝包時，請保留上述授權文件、字庫來源與轉換工具。更新相依套件時，同步檢查其條款、鎖定檔與產物中的聲明。
