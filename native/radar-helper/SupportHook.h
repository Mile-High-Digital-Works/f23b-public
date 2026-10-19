// SPDX-License-Identifier: MIT
#pragma once
#include <windows.h>
#include <cstdint>
#include <cstring>

namespace f23radar {
// A missile asks its launcher LinkToTarget(target) each frame. The Hornet answers with
// avLinkToTargetResponder::is_tracking (FA18C 2.9.30.28738 RVA 0x49d7b0). Native support
// covers the L&S, designated tracks and TWS tracks that pass a fire-control-quality gate.
// The F-23B extension keeps the native answer and the native power/operate/inhibit gates.
// It also accepts a missile's own target while that target remains an MSI trackfile.
// It never creates tracks, changes the L&S or locks, and only answers for the F-23B radar.
using TrackingFunction = bool(*)(void*, unsigned, unsigned);
constexpr uintptr_t responderOffset = 0xb8;   // radar -> avLinkToTargetResponder base
constexpr uintptr_t trackingSlot = 0x635ac8;  // responder vtable slot 1 (is_tracking)
constexpr uintptr_t nativeTracking = 0x49d7b0;
constexpr size_t msiTrackStride = 0x228;      // same vector as the friendly-ranking hook

template<class T> T supportField(const void* p, size_t offset) {
    T value; std::memcpy(&value, static_cast<const char*>(p)+offset, sizeof(value)); return value;
}

// The three gates native is_tracking applies before any target test:
// radar consumer powered (+0x25c0 slot 0x10), operate state 2 (+0x44d0),
// and the inhibit wire (+0x1118 slot 0x58) unpowered.
template<class Access>
bool nativeGatesOpen(void* responder, Access access) {
    auto call = [&](size_t embedded, size_t slot, bool& out) {
        auto object = static_cast<char*>(responder)+embedded;
        if (!access(object, 8, false)) return false;
        auto table = supportField<void*>(object, 0);
        if (!access(table, slot+8, false)) return false;
        auto address = supportField<void*>(table, slot);
        if (!access(address, 1, false)) return false;
        bool (*function)(void*); std::memcpy(&function, &address, sizeof(function));
        out = function(object);
        return true;
    };
    bool powered = false, inhibited = true;
    if (!access(responder, 0x44d4, false)) return false;
    if (!call(0x25c0, 0x10, powered) || !powered) return false;
    if (supportField<int>(responder, 0x44d0) != 2) return false;
    if (!call(0x1118, 0x58, inhibited) || inhibited) return false;
    return true;
}

// The mission computer links to its radar through +0x3d30; the MSI vector is +0x4028..+0x4030.
template<class Access>
bool linkedToRadar(void* mc, void* radar, Access access) {
    if (!mc || !radar || !access(mc, 0x4038, false)) return false;
    auto link = supportField<void*>(mc, 0x3d30);
    return access(link, 0x39, false) && supportField<unsigned char>(link, 0x38)
        && supportField<void*>(link, 0x30) == radar;
}

template<class Access>
bool msiTrackfile(void* mc, void* radar, unsigned target, Access access) {
    if (!target || !linkedToRadar(mc, radar, access)) return false;
    const auto first = supportField<uintptr_t>(mc, 0x4028);
    const auto end = supportField<uintptr_t>(mc, 0x4030);
    if (end < first || (end-first)%msiTrackStride || (end-first)/msiTrackStride > 4096) return false;
    if (end == first || !access(reinterpret_cast<void*>(first), end-first, false)) return false;
    for (auto p = first; p < end; p += msiTrackStride)
        if (supportField<unsigned>(reinterpret_cast<void*>(p), 8) == target) return true;
    return false;
}

// Project decision (October 18, 2026, Stage 27): MALICE support through terminal flight.
// A MALICE datalink block (WeaponBlocks wWCSE_AMRAAM::simulate 0x307a60) asks LinkToTarget at 0x3081e0
// with its target ID (+0x290) and the missile type. On "yes" it sends the target object's own position
// and velocity (0x30821a..0x30826d); the trackfile only decides the answer. Native MSI erases an
// offboard track after 14 s without reports, and the INS then moves the target at constant velocity.
// The seeker search accepts a return only within 600 m of the INS range (isTargetVisible 0x2f02e7).
// On October 17 a Tu-95 sped up after its track was erased, and the seeker never saw it.
// A MALICE that this radar supported keeps support after its trackfile is erased, until impact.
// The native power, operate and inhibit gates still end it.
struct MaliceSupportQuery {
    bool supported = false;                            // Input: this missile's history.
    bool asked = false, granted = false, held = false;  // Outputs of one datalink update.
};
inline bool maliceHoldAllowed(const MaliceSupportQuery& query, bool linked) {
    return linked && query.supported;
}

// One record per live MALICE datalink block. A block that the engine reuses for a new missile starts
// without support history: a gap over 2 s, or another target or launcher, resets the record.
struct SupportHoldRecord {
    static constexpr double staleGap = 2.0;
    unsigned target = 0, launcher = 0;
    double lastSeen = -1, holdStart = -1;
    bool supported = false, holding = false;
    void refresh(unsigned targetId, unsigned launcherId, double now) {
        if (lastSeen < 0 || now < lastSeen || now-lastSeen > staleGap || targetId != target || launcherId != launcher) {
            *this = SupportHoldRecord{};
            target = targetId; launcher = launcherId;
        }
        lastSeen = now;
    }
    // Applies one datalink update. Returns 1 when a hold starts, 2 when it ends, otherwise 0.
    int apply(const MaliceSupportQuery& query, double now) {
        if (!query.asked) return 0;
        if (query.granted) supported = true;
        if (query.held && !holding) { holding = true; holdStart = now; return 1; }
        if (!query.held && holding) { holding = false; return 2; }
        return 0;
    }
};

// Replace one aligned vtable pointer. No instruction bytes change; the swap is one atomic store.
class SlotHook {
    uintptr_t* slot = nullptr;
    uintptr_t stock = 0, replacement = 0;
    bool swap(uintptr_t expected, uintptr_t desired) {
        if (!slot) return false;
        if (*slot == desired) return true;
        DWORD protect = 0, unused = 0;
        if (!VirtualProtect(slot, 8, PAGE_READWRITE, &protect)) return false;
        const bool ok = InterlockedCompareExchange64(reinterpret_cast<volatile LONG64*>(slot),
            static_cast<LONG64>(desired), static_cast<LONG64>(expected)) == static_cast<LONG64>(expected);
        return VirtualProtect(slot, 8, protect, &unused) != FALSE && ok;
    }
public:
    // Accepts only the reviewed slot content: the stock function, or this hook already installed.
    bool prepare(uintptr_t* where, uintptr_t original, void* function) {
        if (!where || (reinterpret_cast<uintptr_t>(where)&7)) return false;
        const auto mine = reinterpret_cast<uintptr_t>(function);
        if (*where != original && *where != mine) return false;
        slot = where; stock = original; replacement = mine;
        return true;
    }
    bool install() { return swap(stock, replacement); }
    bool restore() { return swap(replacement, stock); }
    bool installed() const { return slot && *slot == replacement; }
};
}
