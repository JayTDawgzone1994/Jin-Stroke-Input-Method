# 原生組字測試

MSVC 建置會產生 `tests/Release/stroke_tsf_editor.exe`。這是空白 Windows RichEdit，不開啟／儲存文件。先安裝輸入法、在設定開啟連續模式並套用，再啟動測試程式，按 Activate Jin Stroke；啟用僅作用於該測試程序。

按 `A 空白 A 空白` 確認「一一」整串保留底線。左鍵、下鍵、2 應變成「一有」，仍保留底線；Enter 才取消底線。也應測試退格、Esc、換頁、Shift 英文、長字串與失焦。關閉測試視窗不會儲存文字。
