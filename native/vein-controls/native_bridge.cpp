// SPDX-License-Identifier: MIT
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <MinHook.h>
#include <array>
#include <cstring>
#include <vector>
#include "native_bridge.h"
#include "vein_radius.hpp"
extern "C" unsigned GetVeinMiningRadius();
namespace {
INIT_ONCE initialization=INIT_ONCE_STATIC_INIT;
bool radius_ready=false;
bool verified_file(HMODULE module) {
    wchar_t path[32768]{};
    const DWORD length=GetModuleFileNameW(module,path,32768);
    if (!length || length>=32768) return false;
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if (file==INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    bool ok=GetFileSizeEx(file,&size) && (size.QuadPart==290816 || size.QuadPart==293888);
    std::vector<unsigned char> bytes(ok?static_cast<size_t>(size.QuadPart):0);
    DWORD copied=0;
    ok=ok && ReadFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&copied,nullptr) && copied==bytes.size();
    CloseHandle(file);
    if (!ok || std::memcmp(bytes.data()+11985,"\0\0\x04\0",4)) return false;
    // Normalize only the existing rebinding/localization variants and our
    // bounded region capacity. Everything else must match a complete pin.
    std::memcpy(bytes.data()+11985,"\0\0\x01\0",4);
    const char* input=reinterpret_cast<const char*>(bytes.data()+288536);
    if (std::memcmp(input,"vmkeys.dll",11) && std::memcmp(input,"USER32.dll",11)) return false;
    std::memcpy(bytes.data()+288536,"USER32.dll",11);
    DWORD key=0;
    for (size_t offset:{size_t(7248),size_t(11374),size_t(29096)}) {
        DWORD actual=0;std::memcpy(&actual,bytes.data()+offset+1,4);
        if (!actual || actual>254 || (key && key!=actual)) return false;
        key=actual;const DWORD original=192;std::memcpy(bytes.data()+offset+1,&original,4);
    }
    BCRYPT_ALG_HANDLE algorithm=nullptr;
    BCRYPT_HASH_HANDLE hash=nullptr;
    std::array<unsigned char,32> digest{};
    ok=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0;
    if(ok) ok=BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0;
    if(ok) ok=BCryptHashData(hash,bytes.data(),static_cast<ULONG>(bytes.size()),0)>=0;
    if(ok) ok=BCryptFinishHash(hash,digest.data(),static_cast<ULONG>(digest.size()),0)>=0;
    if(hash) BCryptDestroyHash(hash);
    if(algorithm) BCryptCloseAlgorithmProvider(algorithm,0);
    char hex[65]{};constexpr char digits[]="0123456789abcdef";
    for(size_t i=0;i<digest.size();++i) {hex[i*2]=digits[digest[i]>>4];hex[i*2+1]=digits[digest[i]&15];}
    return ok && (!std::strcmp(hex,"6d31c439fda536aad2078683a244e2d05f685e5e3fba2d5467950fff613f5bcc") ||
        !std::strcmp(hex,"6e1d953a09000915ed0218ff4b947b7abc9707e1c027fdddf5a72f65036d23f1"));
}
BOOL CALLBACK initialize(PINIT_ONCE,void* context,void**) {
    HMODULE module=nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        static_cast<LPCWSTR>(context),&module) || !verified_file(module)) return TRUE;
    HMODULE pinned=nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(module),&pinned)) return TRUE;
    const auto status=MH_Initialize();
    if (status!=MH_OK && status!=MH_ERROR_ALREADY_INITIALIZED) return TRUE;
    if (!vein_radius::prepare(module,GetVeinMiningRadius)) return TRUE;
    const bool ready=MH_ApplyQueued()==MH_OK;
    if (!ready) MH_DisableHook(MH_ALL_HOOKS);
    radius_ready=ready;
    using Logger=void(*)(const char*);
    Logger logger=nullptr;std::memcpy(&logger,reinterpret_cast<unsigned char*>(module)+0x495b8,sizeof(logger));
    if (logger) logger(ready?"[Vein Radius] Ready: F12 cycles 1x/2x/3x; paired radius server required.":
        "[Vein Radius] Hook activation failed; radius hooks disabled.");
    return TRUE;
}
}
bool initialize_radius_bridge(void* caller) {
    InitOnceExecuteOnce(&initialization,initialize,caller,nullptr);
    return radius_ready;
}
