# 安裝程式邊界

`main.cpp` 已提供 C++ 命令列 `stroke_setup.exe`，由 src/windows/CMakeLists.txt 在 MSVC 建置。支援 selftest、register、unregister、enable、disable、status、activate、verify-registered。

register／unregister 需要管理員權限；enable／disable 使用目前使用者。建置不會自動執行任何註冊。完整命令、版本目錄與移除順序見 [Windows 輸入法文件](../../docs/windows-ime.md)。

一般使用者使用 packaging/StrokeIME.iss 產生的安裝精靈，參見 [安裝包文件](../../docs/installer.md)。本 EXE 作為精靈內部的 C++ 註冊與診斷工具。數位簽章與自動更新尚未提供。
