// SPDX-License-Identifier: MIT
// Offline checks for the F-23B missile-support extension. Native services are fixtures.
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "SupportHook.h"

static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #x); ++failures; } } while (0)

static bool access(const void* p, size_t n, bool) {
    auto current = reinterpret_cast<uintptr_t>(p), end = current+n;
    if (!current || end < current) return false;
    while (current < end) {
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(reinterpret_cast<void*>(current), &info, sizeof(info))
            || info.State != MEM_COMMIT || (info.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
        current = reinterpret_cast<uintptr_t>(info.BaseAddress)+info.RegionSize;
    }
    return true;
}
template<class T> static void put(void* p, size_t offset, T value) { std::memcpy(static_cast<char*>(p)+offset, &value, sizeof(value)); }

static bool powered = true, inhibited = false;
static bool consumerPowered(void*) { return powered; }
static bool wireInhibited(void*) { return inhibited; }
static void* consumerTable[4] = {nullptr, nullptr, reinterpret_cast<void*>(&consumerPowered), nullptr};
static void* wireTable[13] = {};

static unsigned nativeTarget = 0;
static bool stockTracking(void*, unsigned target, unsigned) { return target == nativeTarget; }
static bool otherTracking(void*, unsigned, unsigned) { return false; }

int main() {
    wireTable[11] = reinterpret_cast<void*>(&wireInhibited);
    std::vector<char> radar(0x5e68), mc(0x4040), link(0x40), tracks(3*f23radar::msiTrackStride);
    void* responder = radar.data()+f23radar::responderOffset;
    put<void*>(responder, 0x25c0, consumerTable);
    put<void*>(responder, 0x1118, wireTable);
    put<int>(responder, 0x44d0, 2);
    put<void*>(mc.data(), 0x3d30, link.data());
    put<unsigned char>(link.data(), 0x38, 1);
    put<void*>(link.data(), 0x30, radar.data());
    put<uintptr_t>(mc.data(), 0x4028, reinterpret_cast<uintptr_t>(tracks.data()));
    put<uintptr_t>(mc.data(), 0x4030, reinterpret_cast<uintptr_t>(tracks.data()+tracks.size()));
    put<unsigned>(tracks.data(), 8, 16777728u);                              // Su-27, onboard track
    put<unsigned>(tracks.data()+f23radar::msiTrackStride, 8, 16777472u);     // A-50, offboard track
    put<unsigned>(tracks.data()+2*f23radar::msiTrackStride, 8, 16777984u);

    // Gates mirror native: power, operate state 2, inhibit wire off.
    CHECK(f23radar::nativeGatesOpen(responder, access));
    powered = false; CHECK(!f23radar::nativeGatesOpen(responder, access)); powered = true;
    put<int>(responder, 0x44d0, 1); CHECK(!f23radar::nativeGatesOpen(responder, access)); put<int>(responder, 0x44d0, 2);
    inhibited = true; CHECK(!f23radar::nativeGatesOpen(responder, access)); inhibited = false;

    // A missile target that is still a trackfile keeps support; others do not.
    CHECK(f23radar::msiTrackfile(mc.data(), radar.data(), 16777728u, access));
    CHECK(f23radar::msiTrackfile(mc.data(), radar.data(), 16777472u, access));
    CHECK(!f23radar::msiTrackfile(mc.data(), radar.data(), 16777999u, access));   // dropped track
    CHECK(!f23radar::msiTrackfile(mc.data(), radar.data(), 0u, access));
    std::vector<char> otherRadar(0x5e68);
    CHECK(!f23radar::msiTrackfile(mc.data(), otherRadar.data(), 16777728u, access)); // other cockpit
    put<unsigned char>(link.data(), 0x38, 0);
    CHECK(!f23radar::msiTrackfile(mc.data(), radar.data(), 16777728u, access));      // link inactive
    put<unsigned char>(link.data(), 0x38, 1);
    put<uintptr_t>(mc.data(), 0x4030, reinterpret_cast<uintptr_t>(tracks.data()+5)); // misaligned extent
    CHECK(!f23radar::msiTrackfile(mc.data(), radar.data(), 16777728u, access));
    put<uintptr_t>(mc.data(), 0x4030, reinterpret_cast<uintptr_t>(tracks.data()+tracks.size()));
    CHECK(!f23radar::msiTrackfile(nullptr, radar.data(), 16777728u, access));

    // Slot swap: read-only page, stock pointer required, idempotent install, exact restore.
    auto page = static_cast<uintptr_t*>(VirtualAlloc(nullptr, 4096, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE));
    CHECK(page != nullptr);
    const auto stock = reinterpret_cast<uintptr_t>(&stockTracking);
    page[1] = stock;
    DWORD old = 0; VirtualProtect(page, 4096, PAGE_READONLY, &old);
    f23radar::SlotHook hook;
    f23radar::SlotHook wrong;
    CHECK(!wrong.prepare(page+1, 0x1234, reinterpret_cast<void*>(&otherTracking)));   // unexpected content
    CHECK(hook.prepare(page+1, stock, reinterpret_cast<void*>(&otherTracking)));
    CHECK(hook.install() && hook.installed() && page[1] == reinterpret_cast<uintptr_t>(&otherTracking));
    CHECK(hook.install());                                                           // idempotent
    MEMORY_BASIC_INFORMATION info{}; VirtualQuery(page, &info, sizeof(info));
    CHECK(info.Protect == PAGE_READONLY);                                            // protection restored
    CHECK(hook.restore() && page[1] == stock && !hook.installed());
    CHECK(f23radar::SlotHook().prepare(page+1, stock, reinterpret_cast<void*>(&otherTracking)));
    nativeTarget = 7; CHECK(reinterpret_cast<f23radar::TrackingFunction>(page[1])(nullptr, 7, 0));
    VirtualFree(page, 0, MEM_RELEASE);

    // MALICE support hold (Stage 26): only a missile that had support, only before its seeker search,
    // only while the mission computer is linked to this radar.
    f23radar::MaliceSupportQuery query;
    CHECK(!f23radar::maliceHoldAllowed(query, true));                              // never supported
    query.supported = true;
    CHECK(f23radar::maliceHoldAllowed(query, true));
    CHECK(!f23radar::maliceHoldAllowed(query, false));                             // radar link lost
    query.seekerStarted = true;
    CHECK(!f23radar::maliceHoldAllowed(query, true));                              // seeker search started

    // October 17 Tu-95 shot: native support until 426.40 s, trackfile erased at 426.47 s.
    f23radar::SupportHoldRecord record;
    record.refresh(16787712u, 16939264u, 386.5);
    CHECK(!record.supported && !record.holding);
    f23radar::MaliceSupportQuery update;
    CHECK(record.apply(update, 386.5) == 0 && !record.supported);                  // no question this frame
    update.asked = true; update.granted = true;
    CHECK(record.apply(update, 387.0) == 0 && record.supported);                  // native answer
    update = {}; update.asked = true; update.supported = true; update.held = true;
    for (double t = 388.0; t <= 426.5; t += 0.5) record.refresh(16787712u, 16939264u, t);   // live frames
    CHECK(record.supported && record.apply(update, 426.5) == 1 && record.holding && record.holdStart == 426.5);
    CHECK(record.apply(update, 427.0) == 0);                                       // hold continues
    update = {}; update.asked = true; update.supported = true; update.seekerStarted = true;
    for (double t = 427.0; t <= 482.2; t += 0.5) record.refresh(16787712u, 16939264u, t);
    CHECK(record.apply(update, 482.2) == 2 && !record.holding);                    // seeker search: hold ends
    // The hook refreshes the record on every frame of a live missile. A reused block for a new missile
    // (a gap over 2 s, or another target or launcher) starts without history.
    record.refresh(16787712u, 16939264u, 482.25); CHECK(record.supported);
    record.refresh(16787712u, 16939264u, 490.0); CHECK(!record.supported);       // 7.75 s without a frame
    f23radar::SupportHoldRecord reused;
    reused.refresh(1u, 2u, 10.0); update = {}; update.asked = true; update.granted = true; reused.apply(update, 10.0);
    CHECK(reused.supported);
    reused.refresh(1u, 2u, 11.5); CHECK(reused.supported);                         // 1.5 s gap: same missile
    reused.refresh(1u, 2u, 14.0); CHECK(!reused.supported);                        // 2.5 s gap: reset
    reused.apply(update, 14.0); reused.refresh(3u, 2u, 14.1); CHECK(!reused.supported);  // other target
    reused.apply(update, 14.1); reused.refresh(3u, 4u, 14.2); CHECK(!reused.supported);  // other launcher
    reused.apply(update, 14.2); reused.refresh(3u, 4u, 13.0); CHECK(!reused.supported);  // clock went back

    if (failures) return EXIT_FAILURE;
    std::printf("PASS: support gates, MSI trackfile lookup, scope rejection, vtable slot swap and MALICE hold\n");
    return EXIT_SUCCESS;
}
