// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

namespace f23radar {
using RankFunction = void(*)(void*);
constexpr size_t trackStride = 0x228;

template<class T> T rankField(const void* p, size_t offset) {
    T value; std::memcpy(&value, static_cast<const char*>(p)+offset, sizeof(value)); return value;
}

// These offsets belong only to the hash-checked FA18C 2.9.30.28738 ranker.
// Mask 0x198 is consulted by every eligibility pass. The ranker does not write it.
// Keep the native vector and HAFU data intact; restore masks before returning.
template<class Access, class Coalition>
int rankWithoutFriendlies(void* mc, void* ownerRadar, RankFunction original,
                         Access access, Coalition coalition) {
    auto passthrough = [&] (int result) { original(mc); return result; };
    if (!ownerRadar) return passthrough(0);
    if (!access(mc, 0x4044, false)) return passthrough(-1);
    auto link = rankField<void*>(mc, 0x3d30);
    if (!access(link, 0x39, false)) return passthrough(-1);
    if (!rankField<unsigned char>(link, 0x38)
        || rankField<void*>(link, 0x30) != ownerRadar) return passthrough(0);
    auto first = rankField<uintptr_t>(mc, 0x4028);
    auto end = rankField<uintptr_t>(mc, 0x4030);
    if (end < first || (end-first)%trackStride || (end-first)/trackStride > 4096
        || (end != first && !access(reinterpret_cast<void*>(first), end-first, true)))
        return passthrough(-1);
    struct Saved { void* track; uint32_t mask; };
    struct Restore {
        std::vector<Saved> fields;
        ~Restore() {
            for (const auto& item : fields)
                std::memcpy(static_cast<char*>(item.track)+0x198, &item.mask, sizeof(item.mask));
        }
    } saved;
    // Allocate before modifying anything. No allocation occurs with temporary masks.
    try { saved.fields.reserve((end-first)/trackStride); }
    catch (...) { return passthrough(-1); }
    for (auto p = first; p < end; p += trackStride) {
        auto track = reinterpret_cast<void*>(p);
        const auto id = rankField<unsigned>(track, 8);
        const int side = coalition(id);
        // A verified opposing object stays eligible even if HAFU is stale.
        const bool friendly = side == 1 || (side != 2
            && (rankField<unsigned char>(track, 0x102) == 2
                || rankField<unsigned char>(track, 0x118) == 2));
        if (friendly) saved.fields.push_back({track, rankField<uint32_t>(track, 0x198)});
    }
    const uint32_t excluded = 0;
    for (const auto& item : saved.fields)
        std::memcpy(static_cast<char*>(item.track)+0x198, &excluded, sizeof(excluded));
    original(mc);
    return static_cast<int>(saved.fields.size());
}
}
