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

// Owner decision, October 12, 2026. Two more answers of the same slot, Block II seeker only.
// 1. An air-to-air missile is not a lock candidate. The stock handler accepts missiles, and a second
//    Block II then left the rail with the first Block II as its target (fixture run values-headon2).
//    Other missiles, bombs and every aircraft keep the stock answer from this rule.
// 2. While the radar's designated target is a hostile aircraft, another aircraft is not a lock candidate.
//    Two MiGs 1.3 degrees apart were both inside the seeker cone; the seeker took the one that was not
//    designated (owner flight, October 2). With no hostile designation the stock answer applies.
//    The comparison uses object addresses only. The designated address is never read through.
inline bool aircraftType(const WsType& type) {
    return type.level1 == 1 && (type.level2 == 1 || type.level2 == 2);   // airplane or helicopter
}
inline bool blockIISeeker(const WsType& parent, uint16_t projectBlockII) {
    return projectBlockII && airToAirMissile(parent) && parent.level4 == projectBlockII;
}
inline bool rejectMissile(const WsType& parent, uint16_t projectBlockII, const WsType& object) {
    return blockIISeeker(parent, projectBlockII) && airToAirMissile(object);
}
inline bool rejectUndesignated(const WsType& parent, uint16_t projectBlockII, const WsType& object,
                               const void* candidate, const void* designated) {
    return blockIISeeker(parent, projectBlockII) && designated && candidate != designated && aircraftType(object);
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
