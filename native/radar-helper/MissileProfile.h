// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <cstring>
#include <cmath>
#include <initializer_list>

namespace f23radar {
// MALICE INS and seeker profile for WeaponBlocks 37c61b61... / WeaponsBase 66d48df0... (DCS 2.9.30.28738).
// A missile's wSimulationSystem uses the scheme embedded in its ammunition descriptor
// (+0xd8, or +0xcc8 for the network variant). The scheme holds one descriptor pointer per block.
// wINS_Strapdown::refineTargetData discards every datalink message once INS +0x108 (time since
// the last applied message) exceeds descriptor +0xa0 (3.9 s), and it never recovers.
// INS descriptor +0x60 is the INS operating time (100 s). MALICE flies 70-150 s, so both are
// widened for MALICE only. Seeker FOV +0x70 narrows from 140 to 15 degrees; a definition that already
// declares 15 degrees is accepted, and restoring writes back the value that was loaded.
// Seeker handoff and reference range: the unnamed wGP_AMRAAM block (descriptor vtable RVA 0x3ba288)
// enters seeker search when range < descriptor +0x60 (controlGuidanceMode). The wSN_ARA constructor
// sets the detection threshold 5/(+0xd0)^4, so a 5 m2 target is visible at lock_range_5sqm (+0xd0).
// isSignalOutsideOfMLC scales in-band clutter by doppler_bandwidth (+0x100) / band width.
// MALICE's declared 16 NM active search and 40 km sensor range never reached these unnamed or
// radar-base fields through Lua; the profile applies them in memory, MALICE only.
// Updates still flow only while the launcher supports a real track.
constexpr double stockUpdateWindow = 3.9, maliceUpdateWindow = 60.0;
constexpr double stockInsTime = 100.0, maliceInsTime = 600.0;
constexpr double maliceSensorTime = 600.0;   // MALICE-only sensor op_time: identity check
constexpr double radians = 3.14159265358979323846 / 180.0;
constexpr double stockMaliceFov = 140.0 * radians, maliceFov = 15.0 * radians;
constexpr double stockHandoff = 16000.0, maliceHandoff = 16.0 * 1852.0;          // 29,632 m
constexpr double stockLockRange5 = 18000.0, maliceLockRange5 = 40000.0;
constexpr double stockDopplerBandwidth = 16.0, maliceDopplerBandwidth = 4.0;
constexpr uintptr_t guidanceDescriptorVtable = 0x3ba288;   // WeaponBlocks wGP_AMRAAM_Descriptor

template<class T> T profileField(const void* p, size_t offset) {
    T value; std::memcpy(&value, static_cast<const char*>(p)+offset, sizeof(value)); return value;
}

// MSVC std::string at descriptor +8: size +16, capacity +24, small-string buffer when capacity <= 15.
template<class Access>
bool blockNamed(const void* descriptor, const char* expected, Access access) {
    const auto text = static_cast<const char*>(descriptor)+8;
    if (!access(text, 32, false)) return false;
    const auto size = profileField<size_t>(text, 16), capacity = profileField<size_t>(text, 24);
    const char* data = capacity <= 15 ? text : profileField<const char*>(text, 0);
    const auto length = std::strlen(expected);
    return size == length && access(data, size, false) && std::memcmp(data, expected, size) == 0;
}

// Returns the descriptor of the named block in one scheme, or nullptr.
template<class Access>
void* schemeBlock(void* ammunitionDescriptor, size_t schemeOffset, const char* blockName, Access access) {
    if (!ammunitionDescriptor || !access(static_cast<char*>(ammunitionDescriptor)+schemeOffset, 16, false))
        return nullptr;
    const auto first = profileField<uintptr_t>(ammunitionDescriptor, schemeOffset);
    const auto last = profileField<uintptr_t>(ammunitionDescriptor, schemeOffset+8);
    if (!first || last < first || (last-first)%8 || (last-first)/8 > 128
        || !access(reinterpret_cast<void*>(first), last-first, false)) return nullptr;
    for (auto p = first; p < last; p += 8) {
        auto descriptor = profileField<void*>(reinterpret_cast<void*>(p), 0);
        if (descriptor && access(descriptor, 0x100, false) && blockNamed(descriptor, blockName, access))
            return descriptor;
    }
    return nullptr;
}

// Fallback: walk the weapon-database map used by wGetAmmunitionDescriptorByType (std::map node:
// left +0, parent +8, right +0x10, isnil +0x19, key +0x20, value +0x28). The function's first
// instruction is `mov r9, [rip+disp32]` (4C 8B 0D), which loads the map head. Match only the
// real wsType fields (level1-3 bytes, level4 uint16 at +4) and ignore padding bytes.
template<class Access>
void* findAmmunition(const void* lookupFunction, uint16_t level4, Access access) {
    auto code = static_cast<const unsigned char*>(lookupFunction);
    if (!code || !access(code, 7, false) || code[0] != 0x4c || code[1] != 0x8b || code[2] != 0x0d) return nullptr;
    const auto holder = reinterpret_cast<uintptr_t>(code) + 7 + profileField<int32_t>(code, 3);
    if (!access(reinterpret_cast<void*>(holder), 8, false)) return nullptr;
    auto head = profileField<void*>(reinterpret_cast<void*>(holder), 0);
    if (!access(head, 0x30, false)) return nullptr;
    void* stack[96]; int depth = 0; int visited = 0;
    auto node = profileField<void*>(head, 8);   // root
    while ((node && node != head) || depth) {
        while (node && node != head) {
            if (!access(node, 0x30, false) || profileField<unsigned char>(node, 0x19) || depth >= 96) return nullptr;
            stack[depth++] = node; node = profileField<void*>(node, 0);
        }
        node = stack[--depth];
        if (++visited > 20000) return nullptr;
        auto key = static_cast<const unsigned char*>(node) + 0x20;
        if (key[0] == 4 && key[1] == 4 && key[2] == 7 && profileField<uint16_t>(key, 4) == level4)
            return profileField<void*>(node, 0x28);
        node = profileField<void*>(node, 0x10);
    }
    return nullptr;
}

// The guidance block has no scheme name; find its descriptor by vtable. Ambiguity returns nullptr.
template<class Access>
void* guidanceBlock(void* ammunitionDescriptor, size_t schemeOffset, uintptr_t weaponBlocksBase, Access access) {
    if (!ammunitionDescriptor || !weaponBlocksBase
        || !access(static_cast<char*>(ammunitionDescriptor)+schemeOffset, 16, false)) return nullptr;
    const auto first = profileField<uintptr_t>(ammunitionDescriptor, schemeOffset);
    const auto last = profileField<uintptr_t>(ammunitionDescriptor, schemeOffset+8);
    if (!first || last < first || (last-first)%8 || (last-first)/8 > 128
        || !access(reinterpret_cast<void*>(first), last-first, false)) return nullptr;
    void* found = nullptr;
    for (auto p = first; p < last; p += 8) {
        auto descriptor = profileField<void*>(reinterpret_cast<void*>(p), 0);
        if (descriptor && access(descriptor, 0x68, false)
            && profileField<uintptr_t>(descriptor, 0) == weaponBlocksBase + guidanceDescriptorVtable) {
            if (found) return nullptr;
            found = descriptor;
        }
    }
    return found;
}

struct ProfilePatch { double* where; double stock; double value; };

// Validates identity and exclusivity, then records the INS and seeker fields to change.
// malice/stock are ammunition descriptors from the weapon database (either may be null for stock).
// Codes: 1 ok; -1 not found; -2 identity; -3 not writable; -4/-7/-12 shared with AIM-120C;
// -5/-8/-9/-10/-11 unknown current value; -13 guidance block missing.
constexpr int profilePatchCapacity = 12;
template<class Access>
int planMaliceProfile(void* malice, void* stock, uintptr_t weaponBlocksBase, ProfilePatch (&out)[profilePatchCapacity],
                      int& count, Access access) {
    count = 0;
    auto known = [](double value, double stockValue, double maliceValue) { return value == stockValue || value == maliceValue; };
    for (size_t scheme : {size_t(0xd8), size_t(0xcc8)}) {
        auto sensor = schemeBlock(malice, scheme, "sensor", access);
        auto ins = schemeBlock(malice, scheme, "INS", access);
        if (!sensor || !ins) continue;
        if (!access(sensor, 0x108, true) || profileField<double>(sensor, 0x60) != maliceSensorTime) return -2;
        if (!access(ins, 0xa8, true)) return -3;
        auto guidance = static_cast<char*>(guidanceBlock(malice, scheme, weaponBlocksBase, access));
        if (!guidance) return -13;
        if (!access(guidance, 0x68, true)) return -3;
        // Refuse if the stock AIM-120C would share an INS, seeker or guidance descriptor.
        for (size_t other : {size_t(0xd8), size_t(0xcc8)}) {
            if (stock && schemeBlock(stock, other, "INS", access) == ins) return -4;
            if (stock && schemeBlock(stock, other, "sensor", access) == sensor) return -7;
            if (stock && guidanceBlock(stock, other, weaponBlocksBase, access) == guidance) return -12;
        }
        const double window = profileField<double>(ins, 0xa0), time = profileField<double>(ins, 0x60);
        const double fov = profileField<double>(sensor, 0x70);
        const bool windowKnown = window == stockUpdateWindow || window == maliceUpdateWindow;
        const bool timeKnown = time == stockInsTime || time == maliceInsTime;
        if (!windowKnown || !timeKnown) return -5;
        if (std::fabs(fov - stockMaliceFov) > 1e-9 && std::fabs(fov - maliceFov) > 1e-9) return -8;
        auto seeker = static_cast<char*>(sensor);
        if (!known(profileField<double>(guidance, 0x60), stockHandoff, maliceHandoff)) return -9;
        if (!known(profileField<double>(seeker, 0xd0), stockLockRange5, maliceLockRange5)) return -10;
        if (!known(profileField<double>(seeker, 0x100), stockDopplerBandwidth, maliceDopplerBandwidth)) return -11;
        auto base = static_cast<char*>(ins);
        out[count++] = {reinterpret_cast<double*>(base+0xa0), stockUpdateWindow, maliceUpdateWindow};
        out[count++] = {reinterpret_cast<double*>(base+0x60), stockInsTime, maliceInsTime};
        // The declaration may already carry 15 degrees (the independent MALICE does). Restore what was loaded.
        out[count++] = {reinterpret_cast<double*>(seeker+0x70), fov, maliceFov};
        out[count++] = {reinterpret_cast<double*>(guidance+0x60), stockHandoff, maliceHandoff};
        out[count++] = {reinterpret_cast<double*>(seeker+0xd0), stockLockRange5, maliceLockRange5};
        out[count++] = {reinterpret_cast<double*>(seeker+0x100), stockDopplerBandwidth, maliceDopplerBandwidth};
    }
    return count ? 1 : -1;
}

inline void applyProfile(const ProfilePatch* patches, int count, bool enable) {
    for (int i = 0; i < count; ++i) {
        const double value = enable ? patches[i].value : patches[i].stock;
        std::memcpy(patches[i].where, &value, sizeof(value));
    }
}
}
