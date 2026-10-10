// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "SupportHook.h"

namespace f23radar {
// Read-only support diagnostics for FA18C 2.9.30.28738. They change no support decision.
// MSI trackfile fields (captured image, see the SURV continuity evidence):
//   +0xe8/+0xf0/+0xf8 source report times (model time), +0x101 current contribution present,
//   +0x198 source eligibility mask, +0x19d memory state. Native aging clears a source after 14 s.
enum SupportDecision : int { NativeSupport = 1, ExtensionSupport = 2, GatesClosed = 3, NoTrackfile = 4, NotLinked = 5 };

struct TrackSnapshot {
    bool found = false;
    double report[3] = {0, 0, 0};
    unsigned char fresh = 0, memory = 0;
    uint32_t mask = 0;
};

template<class Access>
const char* msiRecord(void* mc, void* radar, unsigned target, Access access) {
    if (!target || !linkedToRadar(mc, radar, access)) return nullptr;
    const auto first = supportField<uintptr_t>(mc, 0x4028);
    const auto end = supportField<uintptr_t>(mc, 0x4030);
    if (end < first || (end-first)%msiTrackStride || (end-first)/msiTrackStride > 4096) return nullptr;
    if (end == first || !access(reinterpret_cast<void*>(first), end-first, false)) return nullptr;
    for (auto p = first; p < end; p += msiTrackStride)
        if (supportField<unsigned>(reinterpret_cast<void*>(p), 8) == target) return reinterpret_cast<const char*>(p);
    return nullptr;
}

inline TrackSnapshot trackSnapshot(const char* record) {
    TrackSnapshot s;
    if (!record) return s;
    s.found = true;
    for (int i = 0; i < 3; ++i) s.report[i] = supportField<double>(record, 0xe8 + 8*i);
    s.fresh = supportField<unsigned char>(record, 0x101);
    s.memory = supportField<unsigned char>(record, 0x19d);
    s.mask = supportField<uint32_t>(record, 0x198);
    return s;
}

// Keeps the latest decision per supported target and yields one log line at a time:
// immediately when a target's decision changes, otherwise every `period` seconds while queried.
class SupportDiagTable {
    struct Entry { unsigned target = 0; int decision = 0, logged = 0; double seen = -1, logAt = -1; TrackSnapshot snap; };
    static constexpr int size = 8;
    Entry entries[size];
public:
    static constexpr double period = 2.0, quiet = 5.0;
    void note(unsigned target, int decision, const TrackSnapshot& snap, double now) {
        if (!target) return;
        Entry* slot = nullptr;
        for (auto& e : entries) if (e.target == target) { slot = &e; break; }
        if (!slot) for (auto& e : entries) if (!e.target || now - e.seen > quiet) { e = Entry{}; e.target = target; slot = &e; break; }
        if (!slot) return;
        slot->decision = decision; slot->snap = snap; slot->seen = now;
    }
    bool nextLine(double now, char* out, size_t n) {
        for (auto& e : entries) {
            if (!e.target || now - e.seen > quiet) continue;
            if (e.decision == e.logged && e.logAt >= 0 && now - e.logAt < period) continue;
            static const char* names[] = {"?", "native", "extension", "gates-closed", "no-trackfile", "not-linked"};
            const char* name = e.decision >= 1 && e.decision <= 5 ? names[e.decision] : names[0];
            auto age = [&](int i) { return e.snap.report[i] > 0 ? now - e.snap.report[i] : -1.0; };
            std::snprintf(out, n, "target=%u decision=%s trackfile=%d fresh=%d memory=%d mask=0x%x age_e8=%.1f age_f0=%.1f age_f8=%.1f",
                e.target, name, e.snap.found ? 1 : 0, e.snap.fresh, e.snap.memory, e.snap.mask, age(0), age(1), age(2));
            e.logged = e.decision; e.logAt = now;
            return true;
        }
        return false;
    }
};
}
