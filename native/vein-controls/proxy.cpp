// SPDX-License-Identifier: MIT
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <atomic>
#include <string>
#include "presentation.h"

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
vein_controls::Notice notice;
std::atomic<HWND> preview_window{nullptr};
HWND popup_window = nullptr; // Owned by the original overlay's message thread.
bool popup_on = false;
constexpr wchar_t preview_class[] = L"XHLVeinMiningPreviewOverlay";
constexpr wchar_t popup_class[] = L"AeroxVeinMiningToggleNotice";

vein_controls::Mode mode() {
    const static auto value = load_mode();
    return value;
}

HWND focused_game() {
    const HWND window = GetForegroundWindow();
    DWORD pid = 0;
    if (window) GetWindowThreadProcessId(window, &pid);
    return pid == GetCurrentProcessId() ? window : nullptr;
}

void refresh_popup(HWND window) {
    const HWND game = focused_game();
    AcquireSRWLockExclusive(&input_lock);
    const bool visible = notice.visible(GetTickCount64(), game != nullptr);
    const bool on = notice.on;
    ReleaseSRWLockExclusive(&input_lock);
    RECT rect{};
    POINT origin{};
    if (!visible || !GetClientRect(game, &rect) || !ClientToScreen(game, &origin) ||
        IsIconic(game) || rect.right <= 0 || rect.bottom <= 0) {
        ShowWindow(window, SW_HIDE);
        return;
    }
    const int dpi = static_cast<int>(GetDpiForWindow(game));
    const int width = MulDiv(240, dpi ? dpi : 96, 96);
    const int height = MulDiv(38, dpi ? dpi : 96, 96);
    const bool repaint = !IsWindowVisible(window) || popup_on != on;
    popup_on = on;
    SetWindowTextW(window, on ? L"Vein Mining: On" : L"Vein Mining: Off");
    SetWindowPos(window, HWND_TOPMOST, origin.x + (rect.right - width) / 2,
        origin.y + rect.bottom * 2 / 3, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    if (repaint) InvalidateRect(window, nullptr, FALSE);
}

LRESULT CALLBACK popup_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_TIMER && wparam == 1) { refresh_popup(window); return 0; }
    if (message == WM_NCHITTEST) return HTTRANSPARENT;
    if (message == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if (message == WM_ERASEBKGND) return 1;
    if (message == WM_PAINT) {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        if (dc) {
            RECT rect{};
            GetClientRect(window, &rect);
            SetDCBrushColor(dc, RGB(24, 27, 30));
            FillRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
            SetDCBrushColor(dc, popup_on ? RGB(112, 196, 142) : RGB(181, 186, 192));
            FrameRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(239, 242, 244));
            const int dpi = static_cast<int>(GetDpiForWindow(window));
            HFONT font = CreateFontW(-MulDiv(16, dpi ? dpi : 96, 96), 0, 0, 0, FW_MEDIUM,
                FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            HGDIOBJ previous = font ? SelectObject(dc, font) : nullptr;
            DrawTextW(dc, popup_on ? L"Vein Mining: On" : L"Vein Mining: Off", -1, &rect,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            if (previous) SelectObject(dc, previous);
            if (font) DeleteObject(font);
        }
        EndPaint(window, &paint);
        return 0;
    }
    if (message == WM_DESTROY) { KillTimer(window, 1); return 0; }
    return DefWindowProcW(window, message, wparam, lparam);
}

void create_popup() {
    HMODULE self = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&VeinKeyState), &self)) return;
    WNDCLASSEXW cls{};
    cls.cbSize = sizeof(cls);
    cls.lpfnWndProc = popup_proc;
    cls.hInstance = self;
    cls.lpszClassName = popup_class;
    if (!RegisterClassExW(&cls) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return;
    // Uses the original preview thread's existing message pump. Never takes focus.
    popup_window = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
        popup_class, L"Vein Mining", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, self, nullptr);
    if (popup_window && !SetTimer(popup_window, 1, 50, nullptr)) {
        DestroyWindow(popup_window);
        popup_window = nullptr;
    }
    if (!popup_window) UnregisterClassW(popup_class, self);
}
}

// Only the verified Vein Mining module imports this replacement. All other
// Unrelated USER32 calls pass through; no game-wide hook or synthetic input is used.
extern "C" SHORT WINAPI VeinKeyState(int key) {
    const auto current_mode = mode();
    if (current_mode == vein_controls::Mode::Hold || key < 1 || key > 254) return GetAsyncKeyState(key);
    AcquireSRWLockExclusive(&input_lock);
    const SHORT physical = GetAsyncKeyState(key);
    const bool before = activation.latched;
    const SHORT result = activation.sample(current_mode, physical, focused_game() != nullptr);
    notice.changed(current_mode, before, activation.latched, GetTickCount64());
    ReleaseSRWLockExclusive(&input_lock);
    return result;
}

extern "C" HWND WINAPI VeinCreateWindowExW(DWORD ex_style, LPCWSTR class_name, LPCWSTR title,
    DWORD style, int x, int y, int width, int height, HWND parent, HMENU menu, HINSTANCE instance, LPVOID param) {
    // Class identity is verified in the pinned 1.0.33 DLL, independent of localization.
    const bool suppress = vein_controls::hide_preview(mode()) &&
        reinterpret_cast<ULONG_PTR>(class_name) > 0xffff && lstrcmpW(class_name, preview_class) == 0;
    const HWND result = CreateWindowExW(ex_style, class_name, title, suppress ? style & ~WS_VISIBLE : style,
        x, y, width, height, parent, menu, instance, param);
    if (result && suppress) {
        preview_window.store(result);
        if (mode() == vein_controls::Mode::Toggle) create_popup();
    }
    return result;
}

extern "C" BOOL WINAPI VeinShowWindow(HWND window, int command) {
    return ShowWindow(window, window && window == preview_window.load() ? SW_HIDE : command);
}

extern "C" BOOL WINAPI VeinSetWindowPos(HWND window, HWND after, int x, int y, int width, int height, UINT flags) {
    if (window && window == preview_window.load()) flags = (flags & ~SWP_SHOWWINDOW) | SWP_HIDEWINDOW;
    return SetWindowPos(window, after, x, y, width, height, flags);
}

extern "C" BOOL WINAPI VeinDestroyWindow(HWND window) {
    if (window && window == preview_window.load()) {
        // Destroy on the same thread and only forget the handle after success.
        if (GetWindowThreadProcessId(window, nullptr) != GetCurrentThreadId()) return DestroyWindow(window);
        const BOOL destroyed = DestroyWindow(window);
        if (destroyed) {
            preview_window.store(nullptr);
            if (popup_window) {
                const auto instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(popup_window, GWLP_HINSTANCE));
                DestroyWindow(popup_window);
                popup_window = nullptr;
                UnregisterClassW(popup_class, instance);
            }
        }
        return destroyed;
    }
    return DestroyWindow(window);
}
