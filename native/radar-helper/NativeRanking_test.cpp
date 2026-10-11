// SPDX-License-Identifier: MIT
// Offline-only bridge for executing the captured native ranker with production headers.
#include <windows.h>
#include <atomic>
#include "NativeRanking.h"
#include "RankingHook.h"

static f23radar::RankingHook hook;
static f23radar::RankFunction original;
static std::atomic<void*> owner{nullptr};
static int sides[256]{};
static int result;
static bool accessible(const void* p, size_t n, bool write) {
    auto current = reinterpret_cast<uintptr_t>(p), end = current+n;
    if (!current || end < current) return false;
    while (current < end) {
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(reinterpret_cast<void*>(current), &info, sizeof(info))
            || info.State != MEM_COMMIT || (info.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
        if (write && !(info.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY))) return false;
        current = reinterpret_cast<uintptr_t>(info.BaseAddress)+info.RegionSize;
    }
    return true;
}
static void rank(void* mc) {
    result = f23radar::rankWithoutFriendlies(mc, owner.load(), original, accessible,
        [](unsigned id) { return id < 256 ? sides[id] : -1; });
}
extern "C" __declspec(dllexport) int install(uintptr_t base) {
    if (!hook.prepare(base, reinterpret_cast<void*>(&rank))) return 0;
    original = reinterpret_cast<f23radar::RankFunction>(hook.trampoline());
    return hook.install();
}
extern "C" __declspec(dllexport) int restore() { owner.store(nullptr); return hook.restore(); }
extern "C" __declspec(dllexport) void set_owner(void* radar) { owner.store(radar); }
extern "C" __declspec(dllexport) void set_side(unsigned id, int side) { if (id < 256) sides[id] = side; }
extern "C" __declspec(dllexport) int last_result() { return result; }
