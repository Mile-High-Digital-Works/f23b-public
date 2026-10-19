// SPDX-License-Identifier: MIT
#pragma once
#include "MaliceEnergy.h"
#include <array>

namespace f23radar {
constexpr uintptr_t chaffCallbackSlot = 0x3bbcf8, chaffCallbackThunk = 0x2ee4e0;
constexpr uintptr_t terminalGuidance = 0x2c7200, inertialGuidance = 0x2c6e90, seekerControl = 0x2c8e10;
constexpr uintptr_t radarSensorVtable = 0x3bbb40, strapdownVtable = 0x3b90c8;
// Owner's Stage 27 policy: two 2-second reports plus margin. No native descriptor changes.
constexpr double recoveryFreshSeconds = 4.5;

inline bool rejectMaliceChaff(bool maliceSensor, unsigned category, unsigned level4) {
    return maliceSensor && category == 0x100 && level4 == 13;
}
inline bool useMaliceRecovery(bool malice, int mode, bool signal, int sensorMode,
                              unsigned selectedTarget, double correctionAge) {
    return malice && mode == 2 && !signal && sensorMode == 0 && selectedTarget == 0
        && std::isfinite(correctionAge) && correctionAge >= 0 && correctionAge < recoveryFreshSeconds;
}

// Optional identities: failure disables this feature only, not the energy profile or support hold.
struct RecoveryIdentity { void* sensor{}; void* ins{}; };
template<class Access>
RecoveryIdentity planRecovery(void* malice, void* stock, const EnergyIdentity& id, Access access) {
    RecoveryIdentity result;
    if (!malice || !stock) return result;
    const auto offset = reinterpret_cast<uintptr_t>(id.scheme)-reinterpret_cast<uintptr_t>(malice);
    if (offset != 0xd8 && offset != 0xcc8) return result;
    result.sensor = schemeBlock(malice, offset, "sensor", access);
    result.ins = schemeBlock(malice, offset, "INS", access);
    if (!access(result.sensor, 0x108, false)
        || profileField<double>(result.sensor, 0x60) != maliceSensorTime) result.sensor = nullptr;
    if (!access(result.ins, 0xa8, false)) result.ins = nullptr;
    for (size_t other : {size_t(0xd8), size_t(0xcc8)}) {
        if (result.sensor == schemeBlock(stock, other, "sensor", access)) result.sensor = nullptr;
        if (result.ins == schemeBlock(stock, other, "INS", access)) result.ins = nullptr;
    }
    return result;
}

using GuidanceStep = void(*)(void*, double);
// controlSeeker uses the descriptor and system pointers for its wire writes. A bounded copy selects
// its mode-1 branch without changing the live GP mode. All direct field accesses end before 0x4f8.
// Its only local writes (+0x160, +0x163) already occurred in the ordinary mode-2 call this frame.
// Require initialized references, so the copy never repeats child initialization (+0xe0 list).
template<class Access>
bool writeInertialReferences(void* block, double step, GuidanceStep writer, Access access) {
    constexpr size_t extent = 0x4f8;
    if (!writer || !access(block, extent, false) || !profileField<unsigned char>(block, 0x163)) return false;
    alignas(16) std::array<unsigned char, extent> view;
    std::memcpy(view.data(), block, extent);
    const int searchMode = 1;
    std::memcpy(view.data()+0x158, &searchMode, sizeof(searchMode));
    writer(view.data(), step);
    return true;
}
}
