// SPDX-License-Identifier: MIT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <filesystem>
#include <string>
#include <stdexcept>
#include <thread>
#include <array>
#include <cstring>
#include "test_input.h"
#include "presentation.h"
using namespace vein_controls;
int checks = 0;
SHORT physical_key = 0;
SHORT radius_key = 0;
HWND test_foreground = nullptr;
SHORT WINAPI read_test_key(int key) { return key == VK_F12 ? radius_key : physical_key; }
HWND WINAPI read_test_foreground() { return test_foreground; }
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
    ++checks;
}
int fixture_pack(void*,void*) { return 0; }
std::uintptr_t fixture_chain(void*,void*,unsigned) { return 0; }
void initialize_real_bridge(HMODULE helper,const std::filesystem::path& root,const char* fixture) {
    const auto target=root/L"mods/XHL-Vein-Mining/XHL-Vein-Mining.dll";
    std::filesystem::copy_file(fixture,target,std::filesystem::copy_options::overwrite_existing);
    HMODULE mod=LoadLibraryW(target.c_str());check(mod!=nullptr,"load real expanded original for controls fixture");
    auto p=reinterpret_cast<unsigned char*>(mod);
    void* pack=reinterpret_cast<void*>(fixture_pack);
    void* chain=reinterpret_cast<void*>(fixture_chain);
    std::memcpy(p+0x49548,&pack,sizeof(pack));
    std::memcpy(p+0x49588,&chain,sizeof(chain));
    std::memcpy(p+0x495a8,&chain,sizeof(chain));
    std::array<unsigned char,0x410> message{};
    std::array<unsigned char,0x330> input{};input[0x318]=1;
    reinterpret_cast<int(*)(void*,void*)>(p+0x36c0)(message.data(),input.data());
    check(GetProcAddress(helper,"GetVeinMiningRadius")!=nullptr,"bridge initialized through actual pinned caller");
}
int main(int argc, char** argv) {
    try {
        if(argc==3 && std::string(argv[1])=="--restore") {
            HMODULE saved=LoadLibraryA(argv[2]);
            const auto selection=saved?reinterpret_cast<unsigned(*)()>(GetProcAddress(saved,"GetVeinMiningRadius")):nullptr;
            check(selection && selection()==3,"fresh process restores the saved radius");
            FreeLibrary(saved);return 0;
        }
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
        const bool radius_enabled=argc>2 && argv[2][0];
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
        const auto radius = reinterpret_cast<unsigned(*)()>(GetProcAddress(dll, "GetVeinMiningRadius"));
        check(radius && radius() == 2, "first launch defaults to double radius");
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
        check(popup != nullptr, "all modes support the radius notice");
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
        {
            TestImport keyboard(dll, "GetAsyncKeyState", reinterpret_cast<ULONG_PTR>(&read_test_key));
            TestImport focus(dll, "GetForegroundWindow", reinterpret_cast<ULONG_PTR>(&read_test_foreground));
            test_foreground = other;
            if(radius_enabled) initialize_real_bridge(dll,root,argv[2]);
            auto gameplay = [&]() {
                SHORT value = 0;
                std::thread reader([&]() { value = key(VK_F8); });
                reader.join();
                return value;
            };
            physical_key = 0;
            key(VK_F8); // Release arms Toggle on the original preview thread.
            physical_key = down;
            check(key(VK_F8) == (held ? down : 0), "hidden preview must never activate target scanning");
            check(gameplay() == down, "gameplay still activates in every mode");
            physical_key = 0;
            for (int i = 0; i < 100000; ++i)
                check(key(VK_F8) == 0, "sustained idle preview stays inactive with mining enabled");
            check(gameplay() == (held ? 0 : down), "Toggle and Always survive idle preview polling");
            if (mode_name == "toggle") {
                SendMessageW(popup, WM_TIMER, 1, 0);
                wchar_t title[40]{};
                GetWindowTextW(popup, title, 40);
                check(lstrcmpW(title, L"Vein Mining: On") == 0, "preview polling still drives toggle-on notice");
            }
            physical_key = down;
            key(VK_F8);
            check(gameplay() == (mode_name == "toggle" ? 0 : down), "second press disables only Toggle");
            if (mode_name == "toggle") {
                SendMessageW(popup, WM_TIMER, 1, 0);
                wchar_t title[40]{};
                GetWindowTextW(popup, title, 40);
                check(lstrcmpW(title, L"Vein Mining: Off") == 0, "toggle-off notice remains available");
            }
            test_foreground = nullptr;
            check(gameplay() == (held ? down : 0), "background preserves Hold and suspends controller modes");
            test_foreground = other;
            physical_key = 0;
            key(VK_F8);
            physical_key = down;
            key(VK_F8); // Enable Toggle again before checking preview lifetime.
            check(gameplay() == down, "mining remains usable after focus return");
            if(radius_enabled) {
            radius_key = down;
            key(VK_F8);
            check(radius() == 3, "F12 cycles double to triple radius");
            for (int i=0;i<1000;++i) key(VK_F8);
            check(radius() == 3 && gameplay() == down, "held F12 never repeats or changes activation");
            SendMessageW(popup, WM_TIMER, 1, 0);
            wchar_t radius_title[80]{};
            GetWindowTextW(popup, radius_title, 80);
            check(lstrcmpW(radius_title, L"Vein Mining: 3x (15 m)") == 0, "radius notice identifies multiplier and metres");
            check(GetPrivateProfileIntW(L"VeinMining", L"RadiusMultiplier", 0, (config.parent_path()/L"radius.ini").c_str()) == 3,
                "radius preference saved for the next game");
            test_foreground = nullptr;
            radius_key = 0; key(VK_F8);
            radius_key = down; key(VK_F8);
            test_foreground = other; key(VK_F8);
            check(radius() == 3, "background F12 and held focus-return never cycle");
            radius_key = 0; key(VK_F8);
            radius_key = down; key(VK_F8);
            check(radius() == 1, "triple wraps to stock radius");
            radius_key = 0; key(VK_F8);
            radius_key = down; key(VK_F8);
            check(radius() == 2, "stock cycles to double");
            radius_key = 0; key(VK_F8);
            radius_key = down; key(VK_F8);
            SendMessageW(popup, WM_TIMER, 1, 0);
            } else {
                radius_key=down;key(VK_F8);
                check(radius()==2,"helper without radius bridge ignores F12");
                SendMessageW(popup,WM_TIMER,1,0);
                wchar_t unavailable_title[80]{};GetWindowTextW(popup,unavailable_title,80);
                check(std::wstring(unavailable_title).find(L" m)")==std::wstring::npos,
                    "unavailable radius is never announced");
                check(!std::filesystem::exists(config.parent_path()/L"radius.ini"),
                    "unavailable radius does not write a preference");
            }
            BOOL wrong_thread_destroy = TRUE;
            std::thread wrong([&]() { wrong_thread_destroy = destroy(preview); });
            wrong.join();
            check(!wrong_thread_destroy && IsWindow(preview), "wrong-thread destruction cannot clear preview ownership");
            check(key(VK_F8) == (held ? down : 0), "failed destruction keeps preview inactive");
            check(destroy(preview) != 0, "preview destruction succeeds");
            check(key(VK_F8) == down, "destroyed preview no longer suppresses its former thread");
            check(destroy(other) != 0, "unrelated window destruction succeeds");
        }
        check(!IsWindow(preview) && (!popup || !IsWindow(popup)), "notification follows preview lifetime");
        check(UnregisterClassW(cls.lpszClassName, instance) != 0, "fixture class released");
        FreeLibrary(dll);
        if(radius_enabled) {
            std::wstring command=L"\""+std::wstring(exe)+L"\" --restore \""+(root/L"vmkeys.dll").wstring()+L"\"";
            STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION child{};
            check(CreateProcessW(exe,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&child)!=0,
                "launch isolated saved-radius verification process");
            const DWORD wait=WaitForSingleObject(child.hProcess,30000);DWORD exit=1;
            GetExitCodeProcess(child.hProcess,&exit);CloseHandle(child.hThread);CloseHandle(child.hProcess);
            check(wait==WAIT_OBJECT_0 && exit==0,"fresh process restores the saved radius");
        }
        check(std::filesystem::weakly_canonical(root).parent_path() ==
            std::filesystem::weakly_canonical(std::filesystem::temp_directory_path()), "cleanup stays in test temporary directory");
        // Real bridge fixtures pin their modules until process exit. The caller
        // may remove this exact printed fixture directory after this process ends.
        if(!radius_enabled) std::filesystem::remove_all(root);
        else std::cout<<"Fixture retained until process exit: "<<root.string()<<'\n';
        std::cout << checks << " input and overlay checks passed (" << mode_name << ")\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
