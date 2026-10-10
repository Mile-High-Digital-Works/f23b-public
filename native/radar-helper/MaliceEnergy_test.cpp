// SPDX-License-Identifier: MIT
#include <windows.h>
#include <array>
#include <vector>
#include <cstdio>
#include "MaliceEnergy.h"
#include "SupportHook.h"
#include "MaliceLoftHook.h"

static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL %d: %s\n", __LINE__, #x); ++failures; } } while (0)
template<class T> void put(void* p, size_t n, T value) { std::memcpy(static_cast<char*>(p)+n, &value, sizeof(value)); }
static bool access(const void* p, size_t n, bool) {
    auto a = reinterpret_cast<uintptr_t>(p), end = a+n;
    if (!a || end<a) return false;
    while (a<end) {
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(reinterpret_cast<void*>(a), &info, sizeof(info)) || info.State != MEM_COMMIT
            || (info.Protect&(PAGE_NOACCESS|PAGE_GUARD))) return false;
        a = reinterpret_cast<uintptr_t>(info.BaseAddress)+info.RegionSize;
    }
    return true;
}
static std::vector<char> descriptor(const char* name, uintptr_t vt = 0) {
    std::vector<char> d(0xb00); put(d.data(), 0, vt);
    std::memcpy(d.data()+8, name, std::strlen(name));
    put<size_t>(d.data()+8, 16, std::strlen(name)); put<size_t>(d.data()+8, 24, 15);
    return d;
}
static int nativeCalls = 0;
static double native(void*, double) { ++nativeCalls; return 2; }
static double replacement(void*, double) { return 3; }

int main() {
    using namespace f23radar;
    EnergyActions actions;
    actions.record(true); actions.record(false); actions.record(true);
    CHECK(actions.malice == 2 && actions.stock == 1);
    EnergyState pulse;
    pulse.observe(0, 2, 2); CHECK(pulse.seekerAge == -1);
    pulse.observe(0, 1, 8); CHECK(pulse.seekerAge == 8);
    CHECK(!pulse.ignite(0, 0, -1, 77.3955));
    CHECK(!pulse.ignite(2, 0, -1, 77.3955));
    CHECK(!pulse.ignite(3, 1, -1, 77.3955));
    CHECK(!pulse.ignite(3, 0, 38.5, 77.3955));
    CHECK(!pulse.ignite(3, 0, -1, 75));
    CHECK(pulse.ignite(3, 0, -1, 77.3955));
    pulse.observe(0, 1, 40); CHECK(pulse.seekerAge == 8);
    CHECK(!pulse.ignite(3, 0, -1, 77.3955));
    CHECK(!pulse.ignite(3, 2, 15.5, 20));
    CHECK(!pulse.ignite(3, 3, 15.5, 0));
    EnergyState untriggered; CHECK(!untriggered.ignite(3, 0, -1, 77.3955));

    // Recorded native order: simulate(0) has no launch range; the first loft call has it.
    EnergyState deferred;
    CHECK(!deferred.initialize(0, 0, 0));
    CHECK(deferred.loft(.02, 0) == .02 && !deferred.loftStopped && !deferred.loftPlanned);
    CHECK(deferred.initialize(.1, 129406.30856593043, 8000));
    CHECK(deferred.loftPlanned && deferred.loftGain == 1.25 && deferred.loftEnd == 40000);
    CHECK(deferred.loft(.02, 129356.153) == .025 && !deferred.loftStopped);

    EnergyState shortShot; shortShot.initialize(0, 60000, 8000);
    CHECK(shortShot.loft(.02, 60000) == 0 && shortShot.loftStopped);
    EnergyState longShot; longShot.initialize(0, 129000, 8000);
    CHECK(longShot.loftEnd == 40000 && longShot.loftGain == 1.25);
    CHECK(longShot.loft(.02, 90000) == .025);
    CHECK(std::fabs(longShot.loft(.02, 47500)-.0125) < 1e-12);
    CHECK(longShot.loft(.02, 40000) == 0);
    CHECK(longShot.loft(.02, 50000) == 0); // Cutoff cannot restart when the target recedes.
    EnergyState low; low.initialize(0, 144000, 600);
    CHECK(low.loftEnd == 55000 && low.loftGain == 1.25);
    CHECK(low.loft(.02, 55000) == 0 && low.loftStopped);
    EnergyState ramp; ramp.initialize(0, 70000, 4500);
    CHECK(ramp.loftGain == .5 && ramp.loftEnd == 47500);

    // The ceiling suspends the loft while the coasting apex would pass 30 km. It does not latch.
    EnergyState owner; owner.initialize(0, 563000, 10000);
    CHECK(owner.loft(.02, 560000, 20000, 12.0, 2110) == .025);               // First sample sets the reference.
    // Owner shot 1 at 12 s: 22 deg climb at 2,110 m/s from 20 km. Coasting apex is about 49 km.
    CHECK(owner.loft(.02, 559000, 20039.5, 12.05, 2110) == 0 && owner.coastApex(20039.5, 2110) > 45000);
    CHECK(owner.loft(.02, 558000, 20039.5, 12.06, 2110) == 0);              // Under 0.05 s keeps the last climb.
    owner.loft(.02, 557500, 29990, 59.9, 2500);                               // Level sample after the climb.
    CHECK(owner.loft(.02, 557000, 29990, 60, 2500) == .025 && !owner.loftStopped);  // Level below the ceiling.
    CHECK(owner.loft(.02, 556000, 30000, 60.5, 2500) == 0 && !owner.loftStopped);   // At the ceiling.
    CHECK(owner.loft(.02, 555000, 29990, 61, 2500) == .025);                 // Descending below it: resumes.
    CHECK(owner.loft(.02, 40000, 29000, 200, 2000) == 0 && owner.loftStopped);
    CHECK(owner.loft(.02, 60000, 20000, 201, 2000) == 0);                    // The range cutoff still latches.
    // Accepted 129 km test at 10 s: 15 deg at 1,473 m/s from 9.2 km. Coasting apex about 16.6 km: loft continues.
    EnergyState mid; mid.initialize(0, 129000, 8000);
    mid.loft(.02, 128000, 9200, 10.0, 1473);
    CHECK(mid.loft(.02, 127000, 9219.06, 10.05, 1473) == .025 && mid.coastApex(9219.06, 1473) < 18000);
    EnergyState unknown; unknown.initialize(0, 563000, 10000);
    CHECK(unknown.loft(.02, 560000, -1, 1, 1000) == .025 && unknown.climbAge < 0);

    const uintptr_t wb = 0x10000000, bs = 0x20000000;
    auto gp = descriptor("", wb+guidanceDescriptorVtable), boost = descriptor("boost"), march = descriptor("march");
    auto controller = descriptor("controller", bs+0x97968);
    put(boost.data(), 0x60, 385.2765); put(boost.data(), 0x80, 15.0);
    put(march.data(), 0x60, 77.3955); put(march.data(), 0x80, 18.0);
    std::array<char, 0xd00> ammo{}, stock{};
    void* descriptors[] = {gp.data(), boost.data(), march.data(), controller.data()};
    put(ammo.data(), 0xd8, descriptors); put(ammo.data(), 0xe0, descriptors+4);
    EnergyIdentity ids[2]; int count = 0;
    CHECK(planEnergy(ammo.data(), stock.data(), wb, bs, ids, count, access) == 1 && count == 1);
    CHECK(planEnergy(ammo.data(), nullptr, wb, bs, ids, count, access) == -30);
    put(stock.data(), 0xd8, descriptors); put(stock.data(), 0xe0, descriptors+4);
    CHECK(planEnergy(ammo.data(), stock.data(), wb, bs, ids, count, access) == -33);
    stock.fill(0);
    put(march.data(), 0x80, 10.0);
    CHECK(planEnergy(ammo.data(), stock.data(), wb, bs, ids, count, access) == -32);
    put(march.data(), 0x80, 18.0);
    CHECK(planEnergy(ammo.data(), stock.data(), wb, bs, ids, count, access) == 1);

    std::array<char, 0x98> system{};
    std::array<char, 0x400> gpLive{}, boostLive{}, marchLive{}, stockLive{};
    void* blocks[] = {gpLive.data(), boostLive.data(), marchLive.data(), nullptr};
    put(system.data(), 0x90, ammo.data()+0xd8); put(system.data(), 0x10, blocks);
    for (int i=0; i<3; ++i) {
        put(blocks[i], 0, wb+(i ? engineVtable : guidanceVtable));
        put(blocks[i], 8, descriptors[i]); put(blocks[i], 0x10, system.data());
    }
    CHECK(sibling(gpLive.data(), ids[0], march.data(), wb+engineVtable, access) == marchLive.data());
    put(marchLive.data(), 8, boost.data());
    CHECK(!sibling(gpLive.data(), ids[0], march.data(), wb+engineVtable, access));
    put(marchLive.data(), 8, march.data()); put(marchLive.data(), 0x10, stock.data());
    CHECK(!sibling(gpLive.data(), ids[0], march.data(), wb+engineVtable, access));
    put(marchLive.data(), 0x10, system.data());
    put(stockLive.data(), 8, stock.data()); put(stockLive.data(), 0x10, stock.data());
    const auto before = stockLive;
    // Same dispatch contract as the native callback: an unmatched descriptor only calls native.
    if (profileField<void*>(stockLive.data(), 8) != ids[0].march) native(stockLive.data(), 38.5);
    CHECK(nativeCalls == 1 && stockLive == before);
    CHECK(!sibling(stockLive.data(), ids[0], march.data(), wb+engineVtable, access));

    std::string bytes(549, '\0'); put(bytes.data(), 314, 38.5);
    const auto original = bytes;
    CHECK(!delayController(bytes, [](const void*, size_t, const char*) { return false; }) && bytes == original);
    CHECK(delayController(bytes, [](const void*, size_t n, const char* hash) {
        return n == 549 && std::strcmp(hash, controllerRunHash) == 0;
    }));
    CHECK(profileField<double>(bytes.data(), 314) == delayedMarch);
    put(bytes.data(), 314, 38.5); CHECK(bytes == original);

    // Actual pointer exchange and actual instruction interception, with executable native fixtures.
    alignas(8) uintptr_t slot = reinterpret_cast<uintptr_t>(&native);
    SlotHook hook;
    CHECK(!hook.prepare(&slot, 1, reinterpret_cast<void*>(&replacement)));
    CHECK(hook.prepare(&slot, reinterpret_cast<uintptr_t>(&native), reinterpret_cast<void*>(&replacement)));
    CHECK(hook.install()); CHECK(reinterpret_cast<double(*)(void*,double)>(slot)(nullptr, 0) == 3);
    CHECK(hook.restore()); CHECK(reinterpret_cast<double(*)(void*,double)>(slot)(nullptr, 0) == 2);

    auto code = static_cast<unsigned char*>(VirtualAlloc(nullptr, 4096, MEM_RESERVE|MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    CHECK(code);
    if (code) {
        const unsigned char prologue[] = {0x40,0x53,0x48,0x83,0xec,0x60,0x80,0xb9,0x61,0x01,0x00,0x00,0x00};
        // mov rax, double(2); movq xmm0,rax; add rsp,60h; pop rbx; ret.
        const unsigned char tail[] = {0x48,0xb8,0,0,0,0,0,0,0,0x40,0x66,0x48,0x0f,0x6e,0xc0,0x48,0x83,0xc4,0x60,0x5b,0xc3};
        std::memcpy(code, prologue, sizeof(prologue)); std::memcpy(code+13, tail, sizeof(tail));
        FlushInstructionCache(GetCurrentProcess(), code, 64);
        MaliceLoftHook bad;
        code[12] = 1; CHECK(!bad.prepare(reinterpret_cast<uintptr_t>(code)-loftEntry, reinterpret_cast<void*>(&replacement)));
        code[12] = 0;
        MaliceLoftHook loftHook;
        CHECK(loftHook.prepare(reinterpret_cast<uintptr_t>(code)-loftEntry, reinterpret_cast<void*>(&replacement)));
        auto trampoline = reinterpret_cast<double(*)(void*,double)>(loftHook.trampoline());
        auto entry = reinterpret_cast<double(*)(void*,double)>(code);
        CHECK(trampoline(gpLive.data(), 0) == 2);
        CHECK(loftHook.install()); CHECK(entry(gpLive.data(), 0) == 3);
        CHECK(trampoline(gpLive.data(), 0) == 2);
        CHECK(loftHook.restore()); CHECK(!std::memcmp(code, prologue, sizeof(prologue)));
        CHECK(entry(gpLive.data(), 0) == 2);
        CHECK(loftHook.install() && loftHook.restore());
        VirtualFree(code, 0, MEM_RELEASE);
    }
    std::printf("MALICE energy tests: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
