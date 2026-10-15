// SPDX-License-Identifier: MIT
#pragma once
#include <cstring>
#include <stdexcept>

// Replace only this test process's helper imports. No real keyboard input or
// foreground changes are sent to Windows or the game.
struct TestImport {
    ULONG_PTR* slot = nullptr;
    ULONG_PTR original = 0;
    TestImport(HMODULE module, const char* name, ULONG_PTR replacement) {
        auto base = reinterpret_cast<unsigned char*>(module);
        auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
        auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
        auto imports = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base +
            nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
        for (; imports->Name && !slot; ++imports) {
            if (!imports->OriginalFirstThunk) continue;
            auto names = reinterpret_cast<IMAGE_THUNK_DATA*>(base + imports->OriginalFirstThunk);
            auto addresses = reinterpret_cast<IMAGE_THUNK_DATA*>(base + imports->FirstThunk);
            for (; names->u1.AddressOfData; ++names, ++addresses) {
                if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
                auto entry = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData);
                if (std::strcmp(entry->Name, name) == 0) {
                    slot = &addresses->u1.Function;
                    break;
                }
            }
        }
        if (!slot) throw std::runtime_error("test import was not found");
        original = *slot;
        write(replacement);
    }
    void write(ULONG_PTR value) {
        DWORD before = 0, ignored = 0;
        if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &before))
            throw std::runtime_error("test import protection failed");
        *slot = value;
        VirtualProtect(slot, sizeof(*slot), before, &ignored);
    }
    ~TestImport() { write(original); }
    TestImport(const TestImport&) = delete;
    TestImport& operator=(const TestImport&) = delete;
};
