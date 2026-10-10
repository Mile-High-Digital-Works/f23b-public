// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <cstring>

namespace f23radar {
// FA18C.dll 1303ca95...: each SA SCL press halves the SA scale; below 5 NM it wraps to 320 NM.
// Two paths store the scale. The integer path (RVA 0x3fb1d6) loads the wrap with `mov eax, 0x140`.
// The double path (RVA 0x389d8a) loads 320.0 from a constant that six other sites share, and
// stores 320.0 from a `movabs` immediate. The F-23B changes only these three wrap values to 640,
// so its SA scale cycles 40, 20, 10, 5, 640, 320, ... Restoring writes back the stock bytes.
// Stock Hornets without an F-23B session never see the change.
struct SaRangePatch {
    uintptr_t rva;                  // instruction start
    unsigned char prefix[4]; size_t prefixSize;
    size_t valueOffset, valueSize;  // bytes that change inside the instruction
    unsigned char stock[8], extended[8];
};
inline constexpr SaRangePatch saRangePatches[3] = {
    // mov eax, 0x140 -> mov eax, 0x280
    {0x3fb1ec, {0xb8}, 1, 1, 4, {0x40, 0x01, 0x00, 0x00}, {0x80, 0x02, 0x00, 0x00}},
    // movsd xmm3, [rip + disp]: 320.0 at 0x61f678 -> the module's read-only 640.0 at 0x6236b0
    {0x389db0, {0xf2, 0x0f, 0x10, 0x1d}, 4, 4, 4, {0xc0, 0x58, 0x29, 0x00}, {0xf8, 0x98, 0x29, 0x00}},
    // movabs rax, 320.0 -> movabs rax, 640.0
    {0x389db8, {0x48, 0xb8}, 2, 2, 8, {0, 0, 0, 0, 0, 0, 0x74, 0x40}, {0, 0, 0, 0, 0, 0, 0x84, 0x40}},
};
inline constexpr uintptr_t saRangeStockConstant = 0x61f678, saRangeExtendedConstant = 0x6236b0;
inline constexpr double saRangeStockLimit = 320.0, saRangeExtendedLimit = 640.0;

// 1 stock, 2 extended, 0 anything else. `base` is the loaded FA18C.dll image.
inline int saRangeState(const unsigned char* base) {
    double stock{}, extended{};
    std::memcpy(&stock, base + saRangeStockConstant, 8);
    std::memcpy(&extended, base + saRangeExtendedConstant, 8);
    if (stock != saRangeStockLimit || extended != saRangeExtendedLimit) return 0;
    int stockCount = 0, extendedCount = 0;
    for (const auto& p : saRangePatches) {
        if (std::memcmp(base + p.rva, p.prefix, p.prefixSize)) return 0;
        const unsigned char* value = base + p.rva + p.valueOffset;
        if (!std::memcmp(value, p.stock, p.valueSize)) ++stockCount;
        else if (!std::memcmp(value, p.extended, p.valueSize)) ++extendedCount;
        else return 0;
    }
    return stockCount == 3 ? 1 : extendedCount == 3 ? 2 : 0;
}

// Writes one value set. The caller makes the code writable and pauses other threads.
inline void saRangeWrite(unsigned char* base, bool extended) {
    for (const auto& p : saRangePatches)
        std::memcpy(base + p.rva + p.valueOffset, extended ? p.extended : p.stock, p.valueSize);
}
}
