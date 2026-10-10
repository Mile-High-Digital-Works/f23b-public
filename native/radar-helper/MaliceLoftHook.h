// SPDX-License-Identifier: MIT
#pragma once
#include <windows.h>
#include <tlhelp32.h>
#include <array>
#include <cstring>
#include <cstdint>
#include <initializer_list>

namespace f23radar {
// WeaponBlocks calcLoftOmega, RVA 0x2c7090. Fixed, reviewed prologue: no decoder and no support for other native builds.
// Suspended threads must be outside the overwritten instruction boundaries.
class MaliceLoftHook {
    unsigned char* entry = nullptr;
    unsigned char* relay = nullptr;
    alignas(8) long long replacement = 0;
    static constexpr unsigned char prologue[13] =
        {0x40,0x53,0x48,0x83,0xec,0x60,0x80,0xb9,0x61,0x01,0x00,0x00,0x00};
    bool exchange(bool install) {
        if (!entry) return true;
        if (std::memcmp(entry+8, prologue+8, 5)) return false;
        long long stock; std::memcpy(&stock, prologue, 8);
        const auto expected = install ? stock : replacement;
        const auto desired = install ? replacement : stock;
        if (std::memcmp(entry, &desired, 8) == 0) return true;
        if (std::memcmp(entry, &expected, 8) != 0) return false;
        // Collect handles before suspending: heap/loader locks can belong to a paused thread.
        std::array<HANDLE, 2048> handles{};
        size_t count = 0, suspended = 0;
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot == INVALID_HANDLE_VALUE) return false;
        THREADENTRY32 item{}; item.dwSize = sizeof(item);
        bool ok = Thread32First(snapshot, &item) != FALSE;
        const auto process = GetCurrentProcessId(), self = GetCurrentThreadId();
        if (ok) do {
            if (item.th32OwnerProcessID != process || item.th32ThreadID == self) continue;
            if (count == handles.size()) { ok = false; break; }
            HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, item.th32ThreadID);
            if (!thread) { ok = false; break; }
            handles[count++] = thread;
        } while (Thread32Next(snapshot, &item));
        CloseHandle(snapshot);
        if (ok) for (; suspended < count; ++suspended) {
            if (SuspendThread(handles[suspended]) == DWORD(-1)) { ok = false; break; }
        }
        if (ok) for (size_t i = 0; i < suspended; ++i) {
            CONTEXT context{}; context.ContextFlags = CONTEXT_CONTROL;
            if (!GetThreadContext(handles[i], &context)
                || (context.Rip >= reinterpret_cast<uintptr_t>(entry)
                    && context.Rip < reinterpret_cast<uintptr_t>(entry)+13)) { ok = false; break; }
        }
        DWORD protect = 0, unused = 0;
        if (ok) {
            ok = VirtualProtect(entry, 8, PAGE_EXECUTE_READWRITE, &protect) != FALSE;
            if (ok) {
                ok = InterlockedCompareExchange64(reinterpret_cast<volatile LONG64*>(entry), desired, expected) == expected;
                const bool restored = VirtualProtect(entry, 8, protect, &unused) != FALSE;
                const bool flushed = FlushInstructionCache(GetCurrentProcess(), entry, 13) != FALSE;
                ok = ok && restored && flushed;
            }
        }
        while (suspended) ResumeThread(handles[--suspended]);
        for (size_t i = 0; i < count; ++i) CloseHandle(handles[i]);
        return ok;
    }
public:
    void* trampoline() const { return relay ? relay+32 : nullptr; }
    bool prepare(uintptr_t module, void* replacementFunction) {
        auto target = reinterpret_cast<unsigned char*>(module+0x2c7090);
        if (entry) return entry == target;
        if ((reinterpret_cast<uintptr_t>(target)&7) || std::memcmp(target, prologue, 13)) return false;
        SYSTEM_INFO info{}; GetSystemInfo(&info);
        const auto granularity = static_cast<uintptr_t>(info.dwAllocationGranularity);
        const auto aligned = reinterpret_cast<uintptr_t>(target)&~(granularity-1);
        // A nearby relay keeps the entry replacement to one aligned 8-byte operation.
        for (uintptr_t delta = granularity; delta < 0x7fff0000 && !relay; delta += granularity) {
            for (const auto address : {aligned+delta, aligned > delta ? aligned-delta : uintptr_t(0)}) {
                if (!address) continue;
                MEMORY_BASIC_INFORMATION region{};
                if (!VirtualQuery(reinterpret_cast<void*>(address), &region, sizeof(region)) || region.State != MEM_FREE) continue;
                relay = static_cast<unsigned char*>(VirtualAlloc(reinterpret_cast<void*>(address), 4096,
                    MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
                if (relay) break;
            }
        }
        if (!relay) return false;
        auto absoluteJump = [](unsigned char* where, const void* destination) {
            const unsigned char jump[] = {0xff,0x25,0,0,0,0};
            std::memcpy(where, jump, 6); std::memcpy(where+6, &destination, 8);
        };
        absoluteJump(relay, replacementFunction);
        std::memcpy(relay+32, prologue, 13);
        absoluteJump(relay+45, target+13);
        unsigned char patch[8] = {0xe9,0,0,0,0,0x90,0x90,0x90};
        const auto relative = static_cast<int32_t>(reinterpret_cast<intptr_t>(relay)
            - reinterpret_cast<intptr_t>(target)-5);
        std::memcpy(patch+1, &relative, 4); std::memcpy(&replacement, patch, 8);
        DWORD old = 0;
        if (!VirtualProtect(relay, 4096, PAGE_EXECUTE_READ, &old)
            || !FlushInstructionCache(GetCurrentProcess(), relay, 64)) {
            VirtualFree(relay, 0, MEM_RELEASE); relay = nullptr; return false;
        }
        entry = target;
        return true;
    }
    bool install() { return entry && exchange(true); }
    bool restore() { return exchange(false); }
    // Relay stays allocated until process exit: a native call can still be returning.
};
}
