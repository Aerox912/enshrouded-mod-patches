// SPDX-License-Identifier: MIT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <stdexcept>
#include "activation.h"
using namespace vein_controls;
int checks = 0;
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
    ++checks;
}
int main() {
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
        HMODULE dll = LoadLibraryW(L"vmkeys.dll");
        check(dll != nullptr, "input helper loads without the game");
        const auto key = reinterpret_cast<SHORT(WINAPI*)(int)>(GetProcAddress(dll, "GetAsyncKeyState"));
        const auto foreground = reinterpret_cast<HWND(WINAPI*)()>(GetProcAddress(dll, "GetForegroundWindow"));
        check(key && foreground, "input and forward exports resolve");
        check(key(0) == GetAsyncKeyState(0), "default helper behavior passes through");
        check(foreground() == GetForegroundWindow(), "window API forwards to the real USER32");
        FreeLibrary(dll);
        std::cout << checks << " input checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
