// SPDX-License-Identifier: MIT
#pragma once
#include <windows.h>
#include <cstdint>
#include <cstring>

namespace f23radar {
// Change one export address before a later module binds its imports. Existing
// callers retain the original entry. The relay preserves the native signature.
// Call only from ordinary initialization, never a loader notification.
class PayloadAccessHook {
    DWORD* slot = nullptr;
    DWORD original = 0, replacement = 0;
    unsigned char* relay = nullptr;
    bool swap(DWORD expected, DWORD desired) {
        if (!slot) return false;
        if (*slot == desired) return true;
        DWORD protection{}, unused{};
        if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &protection)) return false;
        const bool changed = static_cast<DWORD>(InterlockedCompareExchange(
            reinterpret_cast<volatile LONG*>(slot), static_cast<LONG>(desired),
            static_cast<LONG>(expected))) == expected;
        return VirtualProtect(slot, sizeof(*slot), protection, &unused) != FALSE && changed;
    }
public:
    bool prepare(HMODULE module, const char* name, const void* expected, const void* hook) {
        if (slot || !module || !name || !expected || !hook) return false;
        const auto base = reinterpret_cast<uintptr_t>(module);
        const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
        const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) return false;
        const auto directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        if (!directory.VirtualAddress || directory.Size < sizeof(IMAGE_EXPORT_DIRECTORY)) return false;
        const auto exports = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(base + directory.VirtualAddress);
        const auto names = reinterpret_cast<const DWORD*>(base + exports->AddressOfNames);
        const auto ordinals = reinterpret_cast<const WORD*>(base + exports->AddressOfNameOrdinals);
        auto functions = reinterpret_cast<DWORD*>(base + exports->AddressOfFunctions);
        DWORD* found = nullptr;
        for (DWORD i = 0; i < exports->NumberOfNames; ++i) {
            if (std::strcmp(reinterpret_cast<const char*>(base + names[i]), name)) continue;
            if (ordinals[i] >= exports->NumberOfFunctions) return false;
            found = functions + ordinals[i];
            break;
        }
        if (!found || (reinterpret_cast<uintptr_t>(found) & 3)
            || base + *found != reinterpret_cast<uintptr_t>(expected)) return false;
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        const uintptr_t granularity = info.dwAllocationGranularity;
        const uintptr_t limit = base + 0xffffffffULL;
        if (limit < base) return false;
        for (uintptr_t address = base; address < limit && !relay;) {
            MEMORY_BASIC_INFORMATION region{};
            if (!VirtualQuery(reinterpret_cast<void*>(address), &region, sizeof(region))) break;
            const auto end = reinterpret_cast<uintptr_t>(region.BaseAddress) + region.RegionSize;
            const auto aligned = (address + granularity - 1) & ~(granularity - 1);
            if (region.State == MEM_FREE && aligned < limit && end >= aligned + 4096) {
                relay = static_cast<unsigned char*>(VirtualAlloc(reinterpret_cast<void*>(aligned), 4096,
                    MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            }
            if (end <= address) break;
            address = end;
        }
        if (!relay) return false;
        const unsigned char jump[] = {0xff, 0x25, 0, 0, 0, 0};
        std::memcpy(relay, jump, sizeof(jump));
        std::memcpy(relay + sizeof(jump), &hook, sizeof(hook));
        DWORD protection{};
        if (!VirtualProtect(relay, 4096, PAGE_EXECUTE_READ, &protection)
            || !FlushInstructionCache(GetCurrentProcess(), relay, 14)) {
            VirtualFree(relay, 0, MEM_RELEASE); relay = nullptr; return false;
        }
        slot = found;
        original = *slot;
        replacement = static_cast<DWORD>(reinterpret_cast<uintptr_t>(relay) - base);
        return true;
    }
    void* address() const { return relay; }
    bool install() { return swap(original, replacement); }
    bool restore() { return swap(replacement, original); }
    // A bound import can outlive restoration. Keep the relay until process exit.
};
}
