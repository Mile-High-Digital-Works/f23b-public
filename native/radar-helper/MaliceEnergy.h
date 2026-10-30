// SPDX-License-Identifier: MIT
#pragma once
#include "MissileProfile.h"
#include <algorithm>
#include <string>

namespace f23radar {
constexpr uintptr_t guidanceVtable = 0x3e55e8, engineVtable = 0x3d7a60;
constexpr uintptr_t guidanceSimulate = 0x2f2fa0, engineSimulate = 0x1ae4a0;
constexpr uintptr_t engineSwitch = 0x1ae550, loftEntry = 0x2ecdc0;
// wWCSE_AMRAAM vtable (export ??_7wWCSE_AMRAAM@@6B@) and simulate (export ?simulate@wWCSE_AMRAAM@@UEAANN@Z).
constexpr uintptr_t wcseVtable = 0x3e7de8, wcseSimulate = 0x32d790;
constexpr double delayedMarch = 1e9;
constexpr const char* controllerRunHash = "f4f764748943e476d4e9f0ddce22c2ed59b0ef4e796b4dda6bec11fe2ae74355";

// wcse is the MALICE datalink descriptor for the support hold. It is null when it is missing or shared
// with the stock AIM-120C; the hold is then off and the energy profile is unchanged.
struct EnergyIdentity { void* scheme{}; void* guidance{}; void* boost{}; void* march{}; void* controller{}; void* wcse{}; };

// The existing missile profile proves ammunition identity. This also rejects shared motors and controllers.
template<class Access>
int planEnergy(void* malice, void* stock, uintptr_t wb, uintptr_t bs,
               EnergyIdentity (&out)[2], int& count, Access access) {
    count = 0;
    if (!stock) return -30;
    for (size_t offset : {size_t(0xd8), size_t(0xcc8)}) {
        auto gp = guidanceBlock(malice, offset, wb, access);
        if (!gp) continue;
        auto boost = schemeBlock(malice, offset, "boost", access);
        auto march = schemeBlock(malice, offset, "march", access);
        auto controller = schemeBlock(malice, offset, "controller", access);
        if (!access(boost, 0x88, false) || !access(march, 0x88, false)
            || !access(controller, 0xae8, false)) return -31;
        if (profileField<uintptr_t>(controller, 0) != bs+0x97968
            || profileField<double>(boost, 0x60) != 386.97363760 || profileField<double>(boost, 0x80) != 15.6
            || profileField<double>(march, 0x60) != 77.39472752 || profileField<double>(march, 0x80) != 17.55)
            return -32;
        auto wcse = descriptorByVtable(malice, offset, wb+wcseDescriptorVtable, access);
        for (size_t other : {size_t(0xd8), size_t(0xcc8)}) {
            if (guidanceBlock(stock, other, wb, access) == gp
                || schemeBlock(stock, other, "boost", access) == boost
                || schemeBlock(stock, other, "march", access) == march
                || schemeBlock(stock, other, "controller", access) == controller) return -33;
            if (wcse && descriptorByVtable(stock, other, wb+wcseDescriptorVtable, access) == wcse) wcse = nullptr;
        }
        out[count++] = {static_cast<char*>(malice)+offset, gp, boost, march, controller, wcse};
    }
    return count ? 1 : -31;
}

// blocksim+0x22a6a..0x22ad9: descriptors and live blocks use the same index.
template<class Access>
void* sibling(void* block, const EnergyIdentity& id, void* descriptor, uintptr_t vtable, Access access) {
    if (!access(block, 0x18, false)) return nullptr;
    auto system = profileField<void*>(block, 0x10);
    if (!access(system, 0x98, false) || profileField<void*>(system, 0x90) != id.scheme) return nullptr;
    const auto first = profileField<uintptr_t>(id.scheme, 0), last = profileField<uintptr_t>(id.scheme, 8);
    if (!first || last < first || (last-first)%8 || (last-first)/8 > 128
        || !access(reinterpret_cast<void*>(first), last-first, false)) return nullptr;
    auto blocks = profileField<void*>(system, 0x10);
    if (!access(blocks, last-first, false)) return nullptr;
    void* found = nullptr;
    for (size_t i = 0; i < (last-first)/8; ++i) {
        if (profileField<void*>(reinterpret_cast<void*>(first), i*8) != descriptor) continue;
        auto candidate = profileField<void*>(blocks, i*8);
        if (found || !access(candidate, 0x138, true)
            || profileField<void*>(candidate, 8) != descriptor
            || profileField<void*>(candidate, 0x10) != system
            || profileField<uintptr_t>(candidate, 0) != vtable) return nullptr;
        found = candidate;
    }
    return found;
}

// Project decision (October 2, 2026): suspend the loft while the coasting apex would pass the ceiling.
// The owner's 384 and 524 km shots climbed 17-23 degrees and reached 57 and 97 km.
// Accepted 80-130 km shots climbed 9-15 degrees to 13-17 km and stay below this limit.
// The 30 km ceiling is a project value set from those recorded flights; no shot has flown it yet.
constexpr double loftCeiling = 30000, gravity = 9.80665;
// Project decision (October 12, 2026): hold the second pulse when the missile is about to arrive.
// On a short shot the seeker is active from launch, so the pulse fired at boost burnout. The owner's
// 21.6 km shot got it at 9.1 km to go at Mach 7, where a 40 g turn has a radius near 12 km.
// Accepted shots lit the pulse 14 to 19 s before the hit (94 km, 122 km and 541 km launches).
// The limit is 10 s of flight at the present closing speed. It is not latched: a target that opens
// the range again can still get the pulse.
constexpr double minimumPulseTime = 10.0;

struct EnergyState {
    double lastAge = -1, seekerAge = -1, loftEnd = 0, loftGain = 0;
    double climbAge = -1, climbAltitude = 0, climbSine = 0;
    double closingAge = -1, closingRange = 0, closing = 0;
    bool fired = false, loftPlanned = false, loftStopped = false, loftReported = false, ceilingReported = false;
    bool withheld = false, withheldReported = false;
    // Closing speed from the believed range, over windows of 0.25 s or more.
    void track(double age, double range) {
        if (!(range > 0) || !std::isfinite(range) || !std::isfinite(age)) return;
        if (closingAge < 0 || age < closingAge) { closingAge = age; closingRange = range; return; }
        if (age-closingAge < 0.25) return;
        closing = (closingRange-range)/(age-closingAge);
        closingAge = age; closingRange = range;
    }
    // Seconds to the target at the measured closing speed, or at the missile's speed before one exists.
    // Unknown inputs give a long time, so they never hold the pulse.
    double timeToGo(double range, double speed) const {
        const double rate = closing > 1 ? closing : speed;
        return range >= 0 && rate > 1 && std::isfinite(range) && std::isfinite(rate) ? range/rate : 1e9;
    }
    bool initialize(double age, double launchRange, double targetAltitude) {
        // The first simulate call can precede the native launch-range initialization.
        // Preserve native loft until readInputData supplies the launch geometry.
        if (!(launchRange > 0) || !std::isfinite(launchRange) || !std::isfinite(targetAltitude)) return false;
        lastAge = age;
        const auto clamp = [](double x) { return std::clamp(x, 0.0, 1.0); };
        loftGain = launchRange <= 60000 ? 0 : launchRange < 80000
            ? (launchRange-60000)/20000 : 1+0.25*clamp((launchRange-80000)/40000);
        loftEnd = 55000-15000*clamp((targetAltitude-1000)/7000);
        loftPlanned = true;
        return true;
    }
    void observe(int before, int after, double age) {
        if (before == 0 && after == 1 && seekerAge < 0) seekerAge = age;
    }
    // range < 0 skips the arrival limit (the caller has no believed range).
    bool ignite(int boostState, int marchState, double ignition, double fuel, double range = -1, double speed = 0) {
        if (fired || seekerAge < 0 || boostState != 3 || marchState != 0
            || ignition != -1 || std::fabs(fuel-77.39472752) > 1e-8) return false;
        if (range >= 0 && timeToGo(range, speed) < minimumPulseTime) { withheld = true; return false; }
        fired = true;
        return true;
    }
    double coastApex(double altitude, double speed) const {
        const double climb = std::max(0.0, climbSine*speed);
        return altitude + climb*climb/(2*gravity);
    }
    // altitude < 0 skips the ceiling limit. The limit suspends the loft; it does not latch it.
    double loft(double nativeOmega, double range, double altitude = -1, double age = -1, double speed = 0) {
        if (!loftPlanned) return nativeOmega;
        if (loftStopped || loftGain == 0 || range <= loftEnd) { loftStopped = true; return 0; }
        if (altitude >= 0 && age >= 0) {
            if (climbAge < 0 || age < climbAge) { climbAge = age; climbAltitude = altitude; }
            else if (age-climbAge >= 0.05 && speed > 1) {
                climbSine = (altitude-climbAltitude)/((age-climbAge)*speed);
                climbAge = age; climbAltitude = altitude;
            }
            if (coastApex(altitude, speed) >= loftCeiling) return 0;
        }
        const double x = std::clamp((range-loftEnd)/15000, 0.0, 1.0);
        return nativeOmega*loftGain*x*x*(3-2*x);
    }
};

// Count every switch call made by this helper. Native controller actions are outside this count.
struct EnergyActions {
    unsigned long long malice = 0, stock = 0;
    void record(bool maliceDescriptor) { if (maliceDescriptor) ++malice; else ++stock; }
};

// Change the audited numeric constant in a dumped function, never the Lua VM's memory layout.
// The full bytecode hash also checks the two instructions that use this constant.
template<class Hash>
bool delayController(std::string& bytes, Hash hash) {
    if (bytes.size() != 549 || profileField<double>(bytes.data(), 314) != 39.1
        || !hash(bytes.data(), bytes.size(), controllerRunHash)) return false;
    std::memcpy(bytes.data()+314, &delayedMarch, sizeof(delayedMarch));
    return true;
}
}
