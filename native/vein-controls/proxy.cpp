// SPDX-License-Identifier: MIT
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <string>
#include "activation.h"

extern "C" SHORT WINAPI VeinKeyState(int key);

namespace {
vein_controls::Mode load_mode() {
    HMODULE self = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&VeinKeyState), &self)) return vein_controls::Mode::Hold;
    wchar_t filename[32768]{};
    const DWORD length = GetModuleFileNameW(self, filename, 32768);
    if (!length || length >= 32768) return vein_controls::Mode::Hold;
    std::wstring path(filename, length);
    const auto slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return vein_controls::Mode::Hold;
    path.resize(slash + 1);
    path += L"mods\\XHL-Vein-Mining\\controls.ini";
    wchar_t mode[24]{};
    GetPrivateProfileStringW(L"VeinMining", L"Mode", L"hold", mode, 24, path.c_str());
    if (lstrcmpiW(mode, L"toggle") == 0) return vein_controls::Mode::Toggle;
    if (lstrcmpiW(mode, L"always") == 0) return vein_controls::Mode::Always;
    return vein_controls::Mode::Hold;
}
SRWLOCK input_lock = SRWLOCK_INIT;
vein_controls::Activation activation;
}

// Only the verified Vein Mining module imports this replacement. All other
// USER32 functions are forwards; no game-wide hook or synthetic input is used.
extern "C" SHORT WINAPI VeinKeyState(int key) {
    const static auto mode = load_mode();
    if (mode == vein_controls::Mode::Hold || key < 1 || key > 254) return GetAsyncKeyState(key);
    AcquireSRWLockExclusive(&input_lock);
    const SHORT physical = GetAsyncKeyState(key);
    DWORD foreground_pid = 0;
    const HWND foreground = GetForegroundWindow();
    if (foreground) GetWindowThreadProcessId(foreground, &foreground_pid);
    const SHORT result = activation.sample(mode, physical, foreground_pid == GetCurrentProcessId());
    ReleaseSRWLockExclusive(&input_lock);
    return result;
}
