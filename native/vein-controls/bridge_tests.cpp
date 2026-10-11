// SPDX-License-Identifier: MIT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <array>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include "test_input.h"
namespace {
HWND focused=nullptr;
SHORT radius_key=0;
DWORD tick=10000;
unsigned checks=0;
void check(bool ok,const char* text) { if(!ok) throw std::runtime_error(text);++checks; }
HWND WINAPI foreground() { return focused; }
SHORT WINAPI keyboard(int key) { return key==VK_F12?radius_key:0; }
DWORD WINAPI clock_stub() { return tick; }
int pack_stub(void*,void*) { return 732; }
std::uintptr_t chain_stub(void*,void*,unsigned) { return 0; }
template<class T> void put(void* base,size_t offset,T value) {std::memcpy(static_cast<unsigned char*>(base)+offset,&value,sizeof(value));}
template<class T> T get(void* base,size_t offset) {T value;std::memcpy(&value,static_cast<unsigned char*>(base)+offset,sizeof(value));return value;}
}
int wmain(int argc,wchar_t** argv) {
    try {
        check(argc==2 || argc==3,"radius client fixture path required");
        const bool unsupported=argc==3;
        wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,32768);
        const auto root=std::filesystem::temp_directory_path()/(L"vein-bridge-tests-"+std::to_wstring(GetCurrentProcessId()));
        const auto mod=root/L"mods/XHL-Vein-Mining/XHL-Vein-Mining.dll";
        std::filesystem::create_directories(mod.parent_path());
        std::filesystem::copy_file(argv[1],mod,std::filesystem::copy_options::overwrite_existing);
        std::filesystem::copy_file(std::filesystem::path(path).parent_path()/L"vmkeys.dll",root/L"vmkeys.dll",
            std::filesystem::copy_options::overwrite_existing);
        check(WritePrivateProfileStringW(L"VeinMining",L"Mode",L"always",(mod.parent_path()/L"controls.ini").c_str())!=0,"write isolated activation mode");
        HMODULE helper=LoadLibraryW((root/L"vmkeys.dll").c_str());
        check(helper!=nullptr,"load real radius helper");
        HMODULE module=LoadLibraryW(mod.c_str());
        check(module!=nullptr,"load pinned mod with helper import");
        auto p=reinterpret_cast<unsigned char*>(module);
        const auto key=reinterpret_cast<SHORT(WINAPI*)(int)>(GetProcAddress(helper,"GetAsyncKeyState"));
        const auto selection=reinterpret_cast<unsigned(*)()>(GetProcAddress(helper,"GetVeinMiningRadius"));
        check(key&&selection&&selection()==2,"real helper starts at double radius");
        put(p,0x49548,reinterpret_cast<void*>(pack_stub));
        put(p,0x49588,reinterpret_cast<void*>(chain_stub));
        put(p,0x495a8,reinterpret_cast<void*>(chain_stub));
        const DWORD tls=TlsAlloc();check(tls!=TLS_OUT_OF_INDEXES,"allocate fixture mining TLS");put(p,0x370e0,tls);
        TestImport clock(module,"GetTickCount",reinterpret_cast<ULONG_PTR>(clock_stub));
        TestImport input(helper,"GetAsyncKeyState",reinterpret_cast<ULONG_PTR>(keyboard));
        TestImport focus(helper,"GetForegroundWindow",reinterpret_cast<ULONG_PTR>(foreground));
        focused=CreateWindowExW(0,L"STATIC",L"Radius test",WS_POPUP,0,0,1,1,nullptr,nullptr,nullptr,nullptr);
        check(focused!=nullptr,"create owned focus fixture");
        const auto pack=reinterpret_cast<int(*)(void*,void*)>(p+0x36c0);
        std::array<unsigned char,0x410> message{};
        std::array<unsigned char,0x330> player_input{};
        player_input[0x318]=1;
        check(pack(message.data()+16,player_input.data())==732,"first real pack call initializes bridge and preserves return");
        if(unsupported) {
            radius_key=0;key(VK_F8);radius_key=static_cast<SHORT>(0x8000u);key(VK_F8);
            check(selection()==2,"stock-capacity client ignores F12 instead of advertising unavailable radius");
            std::cout<<checks<<" unsupported client rejection checks passed\n";return 0;
        }
        for (unsigned expected:{2u,3u,1u,2u}) {
            message.fill(0);tick+=201;
            check(pack(message.data()+16,player_input.data())==732,"hooked pack preserves original return");
            const DWORD encoded=get<DWORD>(message.data()+16,0x380);
            check(get<DWORD>(message.data()+16,0x37c)==0x434c4858 && (encoded&0xff000000)==0xa6000000 &&
                ((encoded>>22)&3)+1==expected && (encoded&0x3fffff)!=0,
                "actual outbound callback encodes the F12 selection in a versioned request");
            radius_key=0;key(VK_F8);radius_key=static_cast<SHORT>(0x8000u);key(VK_F8);
        }
        message.fill(0);player_input[0x318]=0;tick+=201;
        pack(message.data()+16,player_input.data());
        check(get<DWORD>(message.data()+16,0x37c)==0,"Always enabled without mining sends no request");
        player_input[0x318]=1;focused=nullptr;message.fill(0);tick+=201;
        pack(message.data()+16,player_input.data());
        check(get<DWORD>(message.data()+16,0x37c)==0,"background client sends no request");
        // Module and hook lifetimes intentionally last to process exit, as in game.
        // The parent removes this exact PID fixture only after the process exits.
        std::cout<<checks<<" real client bridge checks passed\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
