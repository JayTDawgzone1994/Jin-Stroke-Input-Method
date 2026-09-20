# WinUI 3 settings, isolated from TSF

The settings executable uses C++20/C++/WinRT and Windows App SDK WinUI 3.
The in-process x64/x86 TSF DLL and candidate UI keep their existing dependencies.
This limits WinUI startup, deployment and UI-thread requirements to the settings process.

`apps/settings/winui/MainView.xaml` owns layout/theme resources. `main.cpp` owns
event handling, draft state, confirmation and error presentation. Both reuse
`layout_store.cpp`, `learning_store.cpp` and the domain key mapping; there is no
second settings format. App-local WinUI and CRT binaries live in a separate
`versions/<version>/settings` directory. The old Win32 settings implementation
is removed rather than maintained as a second UI.

The MSBuild project is independent of CMake's static-CRT IME targets. It compiles
the same small storage sources with the settings executable's dynamic CRT.
NuGet versions/hashes are pinned in packages.lock.json. Restore is explicit;
normal CMake core builds neither restore nor build the WinUI executable.

Runtime XamlReader loads the embedded XAML resource; the Application implements
IXamlMetadataProvider by forwarding to WinUI's metadata provider. No custom
WinRT controls or generated XAML event bindings are needed for this single view.
There is no C# application/runtime dependency. SDK build tooling itself may use .NET.

Raw stroke-key capture uses a current-thread WH_GETMESSAGE hook, scoped to this
window being foreground and an active key/preview button. It does not install
a global keyboard hook. Readable automation names, native controls and theme
resources retain keyboard/accessibility and high-contrast behavior. High-contrast
and screen-reader use still require real-user acceptance testing.

Tradeoff: app-local Windows App SDK increases the settings distribution size.
Deliver the entire output directory, including PRI/DLL and license files; copying
only stroke_settings.exe is insufficient. A clean-machine deployment test remains
required before publishing a new installer.
