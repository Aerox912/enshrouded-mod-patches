// SPDX-License-Identifier: MIT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <filesystem>
#include <string>
#include <stdexcept>
#include "presentation.h"
using namespace vein_controls;
int checks = 0;
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
    ++checks;
}
int main(int argc, char** argv) {
    try {
        constexpr short down = static_cast<short>(0x8000u);
        Activation hold;
        for (const short value : {short(0), short(1), down, short(-32767)})
            check(hold.sample(Mode::Hold, value, false) == value, "hold must preserve the original Win32 result");
        Activation toggle;
        check(toggle.sample(Mode::Toggle, down, true) == 0, "a held key at startup must not arm toggle");
        check(toggle.sample(Mode::Toggle, 0, true) == 0, "initially off");
        check(toggle.sample(Mode::Toggle, down, true) == down, "fresh press turns on");
        for (int i = 0; i < 100; ++i) check(toggle.sample(Mode::Toggle, down, true) == down, "repeated mining and preview reads must not retoggle");
        check(toggle.sample(Mode::Toggle, 0, true) == down, "release keeps mining active");
        check(toggle.sample(Mode::Toggle, down, false) == 0, "background must not mine or toggle");
        check(toggle.sample(Mode::Toggle, down, true) == down, "focus return with held key does not toggle");
        check(toggle.sample(Mode::Toggle, 0, true) == down, "release rearms");
        check(toggle.sample(Mode::Toggle, down, true) == 0, "next fresh press turns off");
        check(toggle.sample(Mode::Toggle, down, true) == 0, "off remains off while held");
        Activation always;
        check(always.sample(Mode::Always, 0, true) == down, "always mode needs no keyboard input");
        check(always.sample(Mode::Always, down, true) == down, "always mode ignores activation presses");
        check(always.sample(Mode::Always, 0, false) == 0, "always mode suspends in background");
        check(!hide_preview(Mode::Hold), "hold retains target-ore preview");
        check(hide_preview(Mode::Toggle) && hide_preview(Mode::Always), "controller modes hide target-ore preview");
        Notice notice;
        check(!notice.visible(100, true), "no startup notice");
        notice.changed(Mode::Toggle, false, true, 100);
        check(notice.visible(100, true) && notice.on, "toggle-on notice");
        notice.changed(Mode::Toggle, true, true, 1200);
        check(notice.visible(1599, true), "notice lasts 1500 milliseconds");
        check(!notice.visible(1600, true), "repeated samples do not extend notice");
        notice.changed(Mode::Toggle, true, false, 1700);
        check(notice.visible(1700, true) && !notice.on, "toggle-off notice");
        notice.changed(Mode::Toggle, false, true, 1800);
        check(notice.visible(1800, true) && notice.on, "rapid toggle replaces notice");
        check(!notice.visible(1801, false) && !notice.visible(1802, true), "focus loss dismisses without replay");
        notice.changed(Mode::Always, false, true, 1900);
        notice.changed(Mode::Hold, false, true, 1900);
        check(!notice.visible(1900, true), "hold and always never announce changes");

        // Exercise the shipped DLL's public Win32 exports in each configured mode.
        // A per-process temporary copy keeps tests independent of the installed game.
        const std::string mode_name = argc > 1 ? argv[1] : "hold";
        check(mode_name == "hold" || mode_name == "toggle" || mode_name == "always", "known test mode");
        wchar_t exe[32768]{};
        check(GetModuleFileNameW(nullptr, exe, 32768) != 0, "find test executable");
        const auto root = std::filesystem::temp_directory_path() /
            (L"vein-controls-tests-" + std::to_wstring(GetCurrentProcessId()));
        const auto config = root / L"mods/XHL-Vein-Mining/controls.ini";
        std::filesystem::create_directories(config.parent_path());
        const std::wstring wide_mode(mode_name.begin(), mode_name.end());
        check(WritePrivateProfileStringW(L"VeinMining", L"Mode", wide_mode.c_str(), config.c_str()) != 0, "write isolated mode");
        std::filesystem::copy_file(std::filesystem::path(exe).parent_path() / L"vmkeys.dll", root / L"vmkeys.dll",
            std::filesystem::copy_options::overwrite_existing);
        HMODULE dll = LoadLibraryW((root / L"vmkeys.dll").c_str());
        check(dll != nullptr, "input helper loads without the game");
        const auto key = reinterpret_cast<SHORT(WINAPI*)(int)>(GetProcAddress(dll, "GetAsyncKeyState"));
        const auto foreground = reinterpret_cast<HWND(WINAPI*)()>(GetProcAddress(dll, "GetForegroundWindow"));
        check(key && foreground, "input and forward exports resolve");
        check(key(0) == GetAsyncKeyState(0), "default helper behavior passes through");
        check(foreground() == GetForegroundWindow(), "window API forwards to the real USER32");
        const auto create = reinterpret_cast<decltype(&CreateWindowExW)>(GetProcAddress(dll, "CreateWindowExW"));
        const auto show = reinterpret_cast<decltype(&ShowWindow)>(GetProcAddress(dll, "ShowWindow"));
        const auto position = reinterpret_cast<decltype(&SetWindowPos)>(GetProcAddress(dll, "SetWindowPos"));
        const auto destroy = reinterpret_cast<decltype(&DestroyWindow)>(GetProcAddress(dll, "DestroyWindow"));
        check(create && show && position && destroy, "overlay exports resolve");
        const auto instance = GetModuleHandleW(nullptr);
        WNDCLASSEXW cls{};
        cls.cbSize = sizeof(cls); cls.lpfnWndProc = DefWindowProcW; cls.hInstance = instance;
        cls.lpszClassName = L"XHLVeinMiningPreviewOverlay";
        check(RegisterClassExW(&cls) != 0, "register pinned preview class fixture");
        HWND preview = create(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, cls.lpszClassName, L"Fixture",
            WS_POPUP | WS_VISIBLE, -32000, -32000, 1, 1, nullptr, nullptr, instance, nullptr);
        check(preview != nullptr, "preview window still created for original mod");
        const bool held = mode_name == "hold";
        check((IsWindowVisible(preview) != 0) == held, "visible creation respects mode");
        show(preview, SW_SHOWNOACTIVATE);
        check((IsWindowVisible(preview) != 0) == held, "ShowWindow cannot restore controller ore bar");
        check(position(preview, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
            SWP_NOACTIVATE | SWP_SHOWWINDOW) != 0, "SetWindowPos succeeds");
        check((IsWindowVisible(preview) != 0) == held, "SetWindowPos cannot restore controller ore bar");
        HWND popup = FindWindowW(L"AeroxVeinMiningToggleNotice", nullptr);
        check((popup != nullptr) == (mode_name == "toggle"), "only toggle has a notification window");
        check(!popup || !IsWindowVisible(popup), "toggle notification starts hidden");
        if (popup) {
            check((GetWindowLongPtrW(popup, GWL_EXSTYLE) & WS_EX_NOACTIVATE) != 0, "popup cannot activate");
            check(SendMessageW(popup, WM_NCHITTEST, 0, 0) == HTTRANSPARENT, "popup ignores pointer input");
        }
        HWND other = create(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, L"STATIC", L"Unrelated", WS_POPUP,
            -32000, -32000, 1, 1, nullptr, nullptr, instance, nullptr);
        check(other != nullptr, "unrelated window created");
        show(other, SW_SHOWNOACTIVATE);
        check(IsWindowVisible(other) != 0, "unrelated windows remain untouched");
        check(destroy(other) != 0 && destroy(preview) != 0, "window destruction succeeds");
        check(!IsWindow(preview) && (!popup || !IsWindow(popup)), "notification follows preview lifetime");
        check(UnregisterClassW(cls.lpszClassName, instance) != 0, "fixture class released");
        FreeLibrary(dll);
        check(std::filesystem::weakly_canonical(root).parent_path() ==
            std::filesystem::weakly_canonical(std::filesystem::temp_directory_path()), "cleanup stays in test temporary directory");
        std::filesystem::remove_all(root);
        std::cout << checks << " input and overlay checks passed (" << mode_name << ")\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
