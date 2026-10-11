// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include "PayloadAlias.h"

namespace f23radar {
// Block II flare rejection in the Hornet cockpit Sidewinder seeker (CockpitBase 2.9.30.28738).
// cockpit::avIRSensor::search (RVA 0x330bd0) asks viObjectManager for nearby objects. Its handler
// (RVA 0x330340) accepts aircraft, helicopters, wing parts, missiles, bombs and the flare type
// {1, 3, 31, 14}. An accepted object becomes a lock candidate when it is nearer than
// seeker->getLockonDistance(object), slot +0x98 of the avSidewinderSeeker avPlatform vtable.
// cockpit::avSidewinderSeeker::search (RVA 0x350bb0) can then move the track to a candidate before
// launch, and the missile leaves the rail with that object as its target.
// The hook answers 0 for a flare while the seeker's parent type (+0x3f0, copied from the
// eqSidewinder type by eqSidewinder::initialize) is the project Block II. A flare then never
// becomes a candidate. Every other object and every other missile type get the stock answer.
constexpr uintptr_t seekerLockDistanceSlot = 0x98, seekerParentType = 0x3f0;

inline bool flareType(const WsType& type) {
    return type.level1 == 1 && type.level2 == 3 && type.level3 == 31 && type.level4 == 14;
}
inline bool rejectDecoy(const WsType& parent, uint16_t projectBlockII, const WsType& object) {
    return projectBlockII && airToAirMissile(parent) && parent.level4 == projectBlockII && flareType(object);
}

// In flight. A legacy IR missile (woMissile, Weapons RVA 0x161ab0) rates each decoy with WeaponsBase
// getIRTargetRelockProbability(candidate, current target, &position, &ccm_k0). Weapons.dll has one call
// (RVA 0x162387) through one import slot, and it passes the missile's own position, missile + 0x1a4.
// The stock answer is ccm_k0 / 20 * IR power ratio. It then adds a global constant when the decoy is
// behind the target on nearly the same line of sight. That constant does not depend on ccm_k0, so a
// head-on shot at a flaring target is decoyed even with ccm_k0 = 0.001.
// The hook answers 0 for a flare when the asking missile is the project Block II. All else is stock.
constexpr uintptr_t weaponsRelockImport = 0x198498, missilePositionOffset = 0x1a4;

// MovingObject::Type() must be `lea rax, [rcx + disp32]; ret`. Returns disp32, or 0 for other code.
inline uint32_t typeAccessorOffset(const unsigned char* code) {
    if (!code || code[0] != 0x48 || code[1] != 0x8d || code[2] != 0x81 || code[7] != 0xc3) return 0;
    uint32_t displacement; std::memcpy(&displacement, code + 3, sizeof(displacement));
    return displacement && displacement < 0x10000 ? displacement : 0;
}
}
