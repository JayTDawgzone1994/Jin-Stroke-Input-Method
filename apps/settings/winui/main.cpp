#include <windows.h>
#undef GetCurrentTime
#include "layout_store.hpp"
#include "learning_store.hpp"
#include <dwmapi.h>
#include <fstream>
#include <microsoft.ui.xaml.window.h>
#include <optional>
#include <shellapi.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.XamlTypeInfo.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Xaml.Interop.h>
using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace stroke;
using namespace stroke::win;
namespace {
std::filesystem::path test_root;
bool self_test{};
int result_code{};
ElementTheme requested_theme{ElementTheme::Default};
const wchar_t* labels[]{L"一  橫／提", L"丨  豎／豎鈎", L"丿  撇",
                        L"丶  點／捺", L"フ  折",       L"＊  任意筆劃"};
struct SettingsApp;
SettingsApp* active{};
struct SettingsApp : ApplicationT<SettingsApp, Markup::IXamlMetadataProvider> {
    XamlTypeInfo::XamlControlsXamlMetaDataProvider metadata;
    Markup::IXamlType GetXamlType(Windows::UI::Xaml::Interop::TypeName const& type) {
        return metadata.GetXamlType(type);
    }
    Markup::IXamlType GetXamlType(hstring const& name) { return metadata.GetXamlType(name); }
    com_array<Markup::XmlnsDefinition> GetXmlnsDefinitions() {
        return metadata.GetXmlnsDefinitions();
    }
    Window window{nullptr};
    FrameworkElement root{nullptr};
    stroke::win::Layout draft, saved;
    bool learning{true}, saved_learning{true}, available{}, updating{}, dialog_open{},
        allow_close{}, preview_active{}, custom_top_horizontal{};
    HWND hwnd{};
    HHOOK hook{};
    std::filesystem::path layout_file, learning_file;
    std::optional<size_t> capture;
    std::string preview;
    std::array<Button, 12> buttons{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                                   nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
    std::array<TextBlock, 12> key_labels{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                                         nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
    template <class T> T control(const wchar_t* name) { return root.FindName(name).as<T>(); }
    void status(const wchar_t* text, InfoBarSeverity severity = InfoBarSeverity::Informational) {
        auto bar = control<InfoBar>(L"Status");
        bar.Message(text);
        bar.Severity(severity);
        bar.IsOpen(true);
    }
    bool dirty() const { return draft != saved || (available && learning != saved_learning); }
    void theme() {
        const BOOL dark = root.ActualTheme() == ElementTheme::Dark;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
        HIGHCONTRASTW contrast{sizeof(contrast)};
        SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0);
        const COLORREF background = (contrast.dwFlags & HCF_HIGHCONTRASTON)
                                        ? DWMWA_COLOR_DEFAULT
                                        : (dark ? RGB(32, 32, 32) : RGB(243, 243, 243));
        const COLORREF foreground = (contrast.dwFlags & HCF_HIGHCONTRASTON)
                                        ? DWMWA_COLOR_DEFAULT
                                        : (dark ? RGB(255, 255, 255) : RGB(0, 0, 0));
        DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &background, sizeof(background));
        DwmSetWindowAttribute(hwnd, DWMWA_TEXT_COLOR, &foreground, sizeof(foreground));
    }
    void update_preview() {
        std::wstring text;
        for (char key : preview) {
            auto value = map_key(layout_config(draft), key);
            text += value ? L"一丨丿丶フ＊"[static_cast<unsigned>(*value) - 1] : L'·';
            text += L' ';
        }
        control<TextBlock>(L"PreviewText").Text(text.empty() ? L"點此開始試打" : text);
    }
    void update_keys() {
        auto keys = layout_keys(draft);
        for (size_t i = 0; i < 12; ++i) {
            std::wstring key(1, keys[i] ? static_cast<wchar_t>(keys[i] - 'a' + 'A') : L'—');
            key_labels[i].Text(capture == i ? L"…" : key);
            Automation::AutomationProperties::SetName(buttons[i],
                                                      std::wstring(i % 2 ? L"右手 " : L"左手 ") +
                                                          labels[i / 2] + L"，按鍵 " + key);
        }
        control<Button>(L"Apply").IsEnabled(valid_layout(draft));
    }
    void render() {
        updating = true;
        capture.reset();
        preview_active = false;
        control<ComboBox>(L"Mode").SelectedIndex(static_cast<int>(draft.mode));
        control<ToggleSwitch>(L"Reverse").IsOn(draft.reverse_selection);
        control<ToggleSwitch>(L"Learning").IsOn(learning);
        control<ToggleSwitch>(L"Learning").IsEnabled(available);
        auto left = control<Grid>(L"LeftKeys"), right = control<Grid>(L"RightKeys");
        left.Children().Clear();
        right.Children().Clear();
        for (int pos = 0; pos < 6; ++pos)
            for (int hand = 0; hand < 2; ++hand) {
                const bool horizontal_top =
                    draft.mode == LayoutMode::traditional ||
                    (draft.mode == LayoutMode::custom && custom_top_horizontal);
                const int stroke = horizontal_top ? pos : (pos + 3) % 6;
                auto button = buttons[static_cast<size_t>(stroke * 2 + hand)];
                Grid::SetRow(button, pos / 3);
                Grid::SetColumn(button, pos % 3);
                (hand ? right : left).Children().Append(button);
            }
        control<TextBlock>(L"KeyHint")
            .Text(draft.mode == LayoutMode::custom
                      ? L"點選按鍵方塊，再按 A–Z 指定；Delete 清除，Esc 取消。"
                      : L"左手 QWE／ASD，右手 UIO／JKL。可複製成自訂配置再調整。");
        update_keys();
        update_preview();
        updating = false;
    }
    void copy(LayoutMode mode) {
        stroke::win::Layout preset;
        preset.mode = mode;
        draft.custom = layout_keys(preset);
        draft.mode = LayoutMode::custom;
        custom_top_horizontal = mode == LayoutMode::traditional;
        render();
    }
    bool apply() {
        if (!valid_layout(draft))
            return false;
        auto layout_result = save_layout(layout_file, draft);
        if (std::holds_alternative<Error>(layout_result)) {
            if (self_test)
                std::ofstream(test_root / L"save-error.txt")
                    << std::get<Error>(layout_result).message << " error=" << GetLastError()
                    << " path=" << layout_file.string();
            status(L"無法儲存按鍵設定，請稍後重試。", InfoBarSeverity::Error);
            return false;
        }
        saved = draft;
        if (available && learning != saved_learning) {
            if (std::holds_alternative<Error>(set_learning_enabled(learning_file, learning))) {
                status(L"按鍵已儲存，但學習設定儲存失敗，請再按一次套用。",
                       InfoBarSeverity::Warning);
                return false;
            }
            saved_learning = learning;
        }
        status(L"已儲存。下次開始輸入時會使用新設定。", InfoBarSeverity::Success);
        return true;
    }
    fire_and_forget clear_records() {
        auto lifetime = get_strong();
        if (dialog_open)
            co_return;
        dialog_open = true;
        try {
            ContentDialog dialog;
            dialog.XamlRoot(root.XamlRoot());
            dialog.Title(box_value(L"清除學習紀錄？"));
            dialog.Content(
                box_value(L"這台電腦累積的選字次數將被清除，無法復原。按鍵配置不會改變。"));
            dialog.PrimaryButtonText(L"清除");
            dialog.CloseButtonText(L"取消");
            dialog.DefaultButton(ContentDialogButton::Close);
            if (co_await dialog.ShowAsync() == ContentDialogResult::Primary) {
                auto state = clear_learning(learning_file);
                if (auto value = std::get_if<LearningState>(&state)) {
                    if (!available)
                        learning = value->enabled;
                    saved_learning = value->enabled;
                    available = true;
                    render();
                    status(L"學習紀錄已清除。", InfoBarSeverity::Success);
                } else
                    status(L"目前無法清除，請稍後再試。", InfoBarSeverity::Error);
            }
        } catch (...) {
            status(L"無法開啟確認視窗。", InfoBarSeverity::Error);
        }
        dialog_open = false;
    }
    fire_and_forget confirm_close() {
        auto lifetime = get_strong();
        if (dialog_open)
            co_return;
        dialog_open = true;
        try {
            ContentDialog dialog;
            dialog.XamlRoot(root.XamlRoot());
            dialog.Title(box_value(L"還有未儲存的變更"));
            dialog.Content(box_value(L"要放棄變更並關閉設定嗎？"));
            dialog.PrimaryButtonText(L"放棄並關閉");
            dialog.CloseButtonText(L"取消");
            dialog.DefaultButton(ContentDialogButton::Close);
            if (co_await dialog.ShowAsync() == ContentDialogResult::Primary) {
                allow_close = true;
                window.Close();
            }
        } catch (...) {
            status(L"無法開啟確認視窗。", InfoBarSeverity::Error);
        }
        dialog_open = false;
    }
    bool key(UINT key) {
        if (dialog_open || key == VK_TAB || key == VK_SHIFT)
            return false;
        if (capture) {
            if (key == VK_ESCAPE)
                capture.reset();
            else if (key >= 'A' && key <= 'Z') {
                draft.custom[*capture] = static_cast<char>(key - 'A' + 'a');
                capture.reset();
            } else if (key == VK_DELETE || key == VK_BACK) {
                draft.custom[*capture] = 0;
                capture.reset();
            }
            update_keys();
            update_preview();
            if (!valid_layout(draft))
                status(L"同一按鍵不能代表不同筆劃，請調整重複的按鍵。", InfoBarSeverity::Warning);
            else if (!capture)
                status(L"按鍵草稿已更新，按「套用變更」儲存。");
            return true;
        }
        if (preview_active) {
            if (key == VK_ESCAPE)
                preview.clear();
            else if (key == VK_BACK) {
                if (!preview.empty())
                    preview.pop_back();
            } else if (key >= 'A' && key <= 'Z' && preview.size() < 32)
                preview += static_cast<char>(key - 'A' + 'a');
            else if (key == VK_RETURN)
                return false;
            update_preview();
            return true;
        }
        return false;
    }
    static LRESULT CALLBACK messages(int code, WPARAM wp, LPARAM lp) {
        if (code >= 0 && wp == PM_REMOVE && active && GetForegroundWindow() == active->hwnd) {
            auto* msg = reinterpret_cast<MSG*>(lp);
            if (msg->message == WM_KEYDOWN && !(GetKeyState(VK_CONTROL) & 0x8000) &&
                !(GetKeyState(VK_MENU) & 0x8000) && !(GetKeyState(VK_LWIN) & 0x8000) &&
                !(GetKeyState(VK_RWIN) & 0x8000)) {
                try {
                    if (active->key(static_cast<UINT>(msg->wParam)))
                        msg->message = WM_NULL;
                } catch (...) {
                }
            }
        }
        return CallNextHookEx(nullptr, code, wp, lp);
    }
    void smoke() {
        try {
            copy(LayoutMode::traditional);
            if (draft.custom[0] != 'q')
                throw std::runtime_error("traditional");
            copy(LayoutMode::standard);
            capture = 0;
            key('Q');
            if (valid_layout(draft))
                throw std::runtime_error("duplicates");
            capture = 0;
            key('Z');
            if (!valid_layout(draft))
                throw std::runtime_error("custom");
            preview_active = true;
            for (char c : std::string("ZSDQWE"))
                key(c);
            if (control<TextBlock>(L"PreviewText").Text() != L"一 丨 丿 丶 フ ＊ ")
                throw std::runtime_error("preview");
            draft.reverse_selection = true;
            learning = false;
            if (!apply())
                throw std::runtime_error("save: " +
                                         to_string(control<InfoBar>(L"Status").Message()));
            auto loaded = load_layout(layout_file);
            if (std::get<stroke::win::Layout>(loaded) != draft)
                throw std::runtime_error("layout roundtrip");
            auto state = std::get<LearningState>(sync_learning(learning_file));
            if (state.enabled || !state.uses.empty() || dirty())
                throw std::runtime_error("learning");
            root.RequestedTheme(ElementTheme::Dark);
            if (root.ActualTheme() != ElementTheme::Dark)
                throw std::runtime_error("dark theme");
            root.RequestedTheme(ElementTheme::Light);
            if (root.ActualTheme() != ElementTheme::Light)
                throw std::runtime_error("light theme");
            root.RequestedTheme(ElementTheme::Default);
            std::ofstream(test_root / L"result.txt")
                << "PASS: presets, custom conflicts, raw keys, stroke preview, reverse order, "
                   "persistence, learning, theme switching\n";
        } catch (std::exception const& e) {
            result_code = 1;
            std::ofstream(test_root / L"result.txt") << "FAIL: " << e.what();
        }
        allow_close = true;
        window.Close();
    }
    void OnLaunched(LaunchActivatedEventArgs const&) {
        try {
            Resources().Insert(box_value(L"ContentControlThemeFontFamily"),
                               Media::FontFamily(L"Microsoft JhengHei UI"));
            Resources().MergedDictionaries().Append(XamlControlsResources{});
            auto module = GetModuleHandleW(nullptr);
            auto resource = FindResourceW(module, MAKEINTRESOURCEW(201), RT_RCDATA);
            if (!resource)
                throw std::runtime_error("Missing settings XAML");
            auto bytes = static_cast<const char*>(LockResource(LoadResource(module, resource)));
            root = Markup::XamlReader::Load(
                       to_hstring(std::string_view(bytes, SizeofResource(module, resource))))
                       .as<FrameworkElement>();
            root.RequestedTheme(requested_theme);
            window = Window();
            window.Title(test_root.empty() ? L"錦筆畫輸入法設定" : L"錦筆畫輸入法設定 — 開發預覽");
            window.Content(root);
            check_hresult(window.as<IWindowNative>()->get_WindowHandle(&hwnd));
            SendMessageW(hwnd, WM_SETICON, ICON_BIG,
                         reinterpret_cast<LPARAM>(LoadIconW(module, MAKEINTRESOURCEW(101))));
            layout_file = test_root.empty() ? layout_path() : test_root / L"layout.dat";
            learning_file = test_root.empty() ? learning_path() : test_root / L"learning.dat";
            auto loaded = load_layout(layout_file);
            if (auto value = std::get_if<stroke::win::Layout>(&loaded))
                draft = *value;
            saved = draft;
            custom_top_horizontal =
                draft.custom[0] == 'q' && draft.custom[2] == 'w' && draft.custom[4] == 'e';
            auto state = sync_learning(learning_file);
            if (auto value = std::get_if<LearningState>(&state)) {
                learning = saved_learning = value->enabled;
                available = true;
            }
            for (size_t i = 0; i < 12; ++i) {
                buttons[i] = Button();
                buttons[i].HorizontalAlignment(HorizontalAlignment::Stretch);
                buttons[i].MinHeight(74);
                StackPanel panel;
                panel.Spacing(4);
                key_labels[i] = TextBlock();
                key_labels[i].FontSize(23);
                panel.Children().Append(key_labels[i]);
                TextBlock label;
                label.Text(labels[i / 2]);
                label.FontSize(12);
                panel.Children().Append(label);
                buttons[i].Content(panel);
                buttons[i].Click([this, i](auto&&, auto&&) {
                    if (draft.mode == LayoutMode::custom) {
                        capture = i;
                        preview_active = false;
                        update_keys();
                    }
                });
                buttons[i].LostFocus([this, i](auto&&, auto&&) {
                    if (capture == i) {
                        capture.reset();
                        update_keys();
                    }
                });
            }
            control<ComboBox>(L"Mode").SelectionChanged([this](auto&&, auto&&) {
                if (!updating) {
                    auto index = control<ComboBox>(L"Mode").SelectedIndex();
                    if (index >= 0 && index <= 2) {
                        draft.mode = static_cast<LayoutMode>(index);
                        if (draft.mode != LayoutMode::custom && !valid_layout(draft))
                            draft.custom = stroke::win::Layout{}.custom;
                        render();
                    }
                }
            });
            control<Button>(L"CopyStandard").Click([this](auto&&, auto&&) {
                copy(LayoutMode::standard);
            });
            control<Button>(L"CopyTraditional").Click([this](auto&&, auto&&) {
                copy(LayoutMode::traditional);
            });
            control<Button>(L"Preview").Click([this](auto&&, auto&&) {
                preview_active = true;
                capture.reset();
                update_keys();
            });
            control<Button>(L"Preview").LostFocus([this](auto&&, auto&&) {
                preview_active = false;
            });
            control<ToggleSwitch>(L"Reverse").Toggled([this](auto&&, auto&&) {
                if (!updating)
                    draft.reverse_selection = control<ToggleSwitch>(L"Reverse").IsOn();
            });
            control<ToggleSwitch>(L"Learning").Toggled([this](auto&&, auto&&) {
                if (!updating)
                    learning = control<ToggleSwitch>(L"Learning").IsOn();
            });
            control<Button>(L"Apply").Click([this](auto&&, auto&&) { apply(); });
            control<Button>(L"Reset").Click([this](auto&&, auto&&) {
                draft = stroke::win::Layout{};
                render();
            });
            control<Button>(L"ClearLearning").Click([this](auto&&, auto&&) { clear_records(); });
            control<Button>(L"Close").Click([this](auto&&, auto&&) { window.Close(); });
            window.AppWindow().Closing([this](auto&&, auto&& args) {
                if (dialog_open && !allow_close) {
                    args.Cancel(true);
                    return;
                }
                if (!allow_close && dirty()) {
                    args.Cancel(true);
                    confirm_close();
                }
            });
            root.ActualThemeChanged([this](auto&&, auto&&) { theme(); });
            window.Closed([this](auto&&, auto&&) {
                if (hook)
                    UnhookWindowsHookEx(hook);
                hook = nullptr;
                active = nullptr;
            });
            active = this;
            hook = SetWindowsHookExW(WH_GETMESSAGE, messages, nullptr, GetCurrentThreadId());
            if (!hook)
                throw std::runtime_error("Cannot initialize keyboard capture");
            render();
            if (std::holds_alternative<Error>(loaded))
                status(L"設定讀取失敗，目前顯示預設配置。套用才會寫入。", InfoBarSeverity::Warning);
            if (!available)
                status(L"目前無法讀取學習設定，請稍後重新開啟。", InfoBarSeverity::Warning);
            MONITORINFO monitor{sizeof(monitor)};
            GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor);
            const int width =
                std::min(MulDiv(900, static_cast<int>(GetDpiForWindow(hwnd)), 96),
                         static_cast<int>(monitor.rcWork.right - monitor.rcWork.left - 32));
            const int height =
                std::min(MulDiv(860, static_cast<int>(GetDpiForWindow(hwnd)), 96),
                         static_cast<int>(monitor.rcWork.bottom - monitor.rcWork.top - 32));
            if (auto presenter = window.AppWindow()
                                     .Presenter()
                                     .try_as<Microsoft::UI::Windowing::OverlappedPresenter>()) {
                presenter.PreferredMinimumWidth(
                    std::min(width, MulDiv(640, static_cast<int>(GetDpiForWindow(hwnd)), 96)));
                presenter.PreferredMinimumHeight(
                    std::min(height, MulDiv(480, static_cast<int>(GetDpiForWindow(hwnd)), 96)));
            }
            window.AppWindow().MoveAndResize(
                {monitor.rcWork.left + (monitor.rcWork.right - monitor.rcWork.left - width) / 2,
                 monitor.rcWork.top + (monitor.rcWork.bottom - monitor.rcWork.top - height) / 2,
                 width, height});
            theme();
            window.Activate();
            if (self_test)
                window.DispatcherQueue().TryEnqueue([this] { smoke(); });
        } catch (hresult_error const& error) {
            result_code = 1;
            if (self_test)
                std::ofstream(test_root / L"result.txt")
                    << std::hex << static_cast<unsigned>(error.code()) << ": "
                    << to_string(error.message());
            else
                MessageBoxW(nullptr, error.message().c_str(), L"設定程式啟動失敗",
                            MB_OK | MB_ICONERROR);
            Exit();
        } catch (std::exception const& error) {
            result_code = 1;
            if (self_test)
                std::ofstream(test_root / L"result.txt") << error.what();
            else
                MessageBoxW(nullptr, to_hstring(error.what()).c_str(), L"設定程式啟動失敗",
                            MB_OK | MB_ICONERROR);
            Exit();
        }
    }
};
} // namespace
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    try {
        int count{};
        auto args = CommandLineToArgvW(GetCommandLineW(), &count);
        if (count >= 3 && (std::wstring_view(args[1]) == L"--preview" ||
                           std::wstring_view(args[1]) == L"--self-test")) {
            self_test = std::wstring_view(args[1]) == L"--self-test";
            test_root = args[2];
            std::filesystem::create_directories(test_root);
            if (count >= 5 && std::wstring_view(args[3]) == L"--theme")
                requested_theme = std::wstring_view(args[4]) == L"dark" ? ElementTheme::Dark
                                                                        : ElementTheme::Light;
        }
        if (count == 1) {
            const auto name = std::filesystem::path(args[0]).filename().wstring();
            if (name == L"stroke_settings_preview.exe" ||
                name == L"stroke_settings_dark_preview.exe") {
                const bool dark = name == L"stroke_settings_dark_preview.exe";
                test_root = std::filesystem::path(args[0]).parent_path() /
                            (dark ? L"preview-dark" : L"preview-light");
                std::filesystem::create_directories(test_root);
                requested_theme = dark ? ElementTheme::Dark : ElementTheme::Light;
            }
        }
        LocalFree(args);
        init_apartment(apartment_type::single_threaded);
        Application app{nullptr};
        Application::Start([&](auto&&) { app = make<SettingsApp>(); });
    } catch (hresult_error const& error) {
        result_code = 1;
        if (self_test)
            std::ofstream(test_root / L"result.txt") << to_string(error.message());
        else
            MessageBoxW(nullptr, error.message().c_str(), L"設定程式啟動失敗",
                        MB_OK | MB_ICONERROR);
    } catch (std::exception const& error) {
        result_code = 1;
        if (self_test)
            std::ofstream(test_root / L"result.txt") << error.what();
        else
            MessageBoxW(nullptr, to_hstring(error.what()).c_str(), L"設定程式啟動失敗",
                        MB_OK | MB_ICONERROR);
    }
    return result_code;
}
