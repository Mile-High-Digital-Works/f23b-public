// SPDX-License-Identifier: MIT
#include <windows.h>
#include "SeekerDecoyHook.h"
#include "SupportHook.h"
#include <cstdio>
#include <cstdlib>
using namespace f23radar;
static void require(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
// A seeker laid out like the native one: vtable pointer first, parent type at +0x3f0.
struct FakeSeeker { uintptr_t* vtable; unsigned char body[0x3f0 - 8]; WsType parent; };
static_assert(offsetof(FakeSeeker, parent) == seekerParentType, "parent type offset");
struct FakeObject { WsType type; };
static const uint16_t project = 475;
static int stockCalls = 0;
static float stock_distance(const void*, void*) { ++stockCalls; return 14000.0f; }
static float hooked_distance(const void* seeker, void* object) {
    WsType parent; std::memcpy(&parent, static_cast<const char*>(seeker) + seekerParentType, sizeof(parent));
    if (object && rejectDecoy(parent, project, static_cast<FakeObject*>(object)->type)) return 0.0f;
    return stock_distance(seeker, object);
}
int main() {
    const WsType flare{1, 3, 31, 0, 14, 0}, chaff{1, 3, 31, 0, 13, 0}, fighter{1, 1, 1, 0, 3, 0};
    const WsType wingPart{1, 19, 0, 0, 0, 0}, missile{4, 4, 7, 0, 136, 0};
    const WsType blockII{4, 4, 7, 0, project, 0}, stock9X{4, 4, 7, 0, 136, 0}, malice{4, 4, 7, 0, 474, 0};
    require(flareType(flare) && !flareType(chaff) && !flareType(fighter) && !flareType(wingPart), "flare identity");
    require(rejectDecoy(blockII, project, flare), "Block II seeker accepted a flare");
    require(!rejectDecoy(blockII, project, fighter) && !rejectDecoy(blockII, project, missile)
        && !rejectDecoy(blockII, project, wingPart) && !rejectDecoy(blockII, project, chaff),
        "Block II seeker rejected a non-flare object");
    require(!rejectDecoy(stock9X, project, flare), "stock AIM-9X seeker changed");
    require(!rejectDecoy(malice, project, flare), "another project type changed");
    require(!rejectDecoy(blockII, 0, flare), "unknown project type rejected a flare");
    const WsType notMissile{1, 4, 7, 0, project, 0};
    require(!rejectDecoy(notMissile, project, flare), "non-missile parent rejected a flare");

    // Air-to-air missiles are not lock candidates for a Block II seeker. Everything else is unchanged.
    const WsType cruise{4, 4, 8, 0, 60, 0}, bomb{4, 5, 9, 0, 31, 0}, helicopter{1, 2, 6, 0, 160, 0};
    require(rejectMissile(blockII, project, missile) && rejectMissile(blockII, project, blockII)
        && rejectMissile(blockII, project, malice), "Block II seeker accepted an air-to-air missile");
    require(!rejectMissile(blockII, project, cruise) && !rejectMissile(blockII, project, bomb)
        && !rejectMissile(blockII, project, fighter) && !rejectMissile(blockII, project, flare)
        && !rejectMissile(blockII, project, wingPart), "missile rule rejected another object");
    require(!rejectMissile(stock9X, project, missile) && !rejectMissile(malice, project, missile)
        && !rejectMissile(blockII, 0, missile) && !rejectMissile(notMissile, project, missile),
        "missile rule changed another seeker");

    // With a designated hostile aircraft, a Block II seeker takes that aircraft only.
    FakeObject leader{fighter}, wingman{fighter}, hind{helicopter}, incoming{cruise};
    require(aircraftType(fighter) && aircraftType(helicopter) && !aircraftType(flare) && !aircraftType(wingPart)
        && !aircraftType(missile) && !aircraftType(cruise), "aircraft identity");
    require(!rejectUndesignated(blockII, project, fighter, &wingman, &wingman), "designated aircraft rejected");
    require(rejectUndesignated(blockII, project, fighter, &leader, &wingman)
        && rejectUndesignated(blockII, project, helicopter, &hind, &wingman), "undesignated aircraft accepted");
    require(!rejectUndesignated(blockII, project, cruise, &incoming, &wingman)
        && !rejectUndesignated(blockII, project, wingPart, &incoming, &wingman), "designation rule rejected a non-aircraft");
    require(!rejectUndesignated(blockII, project, fighter, &leader, nullptr), "no designation changed the seeker");
    require(!rejectUndesignated(stock9X, project, fighter, &leader, &wingman)
        && !rejectUndesignated(blockII, 0, fighter, &leader, &wingman), "designation rule changed another seeker");

    // Slot behaviour: one aligned vtable pointer, stock content required, exact restore.
    uintptr_t table[32]{};
    table[seekerLockDistanceSlot / 8] = reinterpret_cast<uintptr_t>(&stock_distance);
    SlotHook hook;
    require(!SlotHook().prepare(&table[seekerLockDistanceSlot / 8], 0x1234, reinterpret_cast<void*>(&hooked_distance)),
        "unknown slot content accepted");
    require(hook.prepare(&table[seekerLockDistanceSlot / 8], reinterpret_cast<uintptr_t>(&stock_distance),
        reinterpret_cast<void*>(&hooked_distance)) && hook.install() && hook.installed(), "slot not installed");
    FakeSeeker seeker{table, {}, blockII};
    FakeObject flareObject{flare}, fighterObject{fighter};
    auto call = [&](FakeSeeker& s, void* object) {
        return reinterpret_cast<float(*)(const void*, void*)>(s.vtable[seekerLockDistanceSlot / 8])(&s, object);
    };
    require(call(seeker, &flareObject) == 0.0f && stockCalls == 0, "flare reached the stock lock distance");
    require(call(seeker, &fighterObject) == 14000.0f && stockCalls == 1, "aircraft did not get the stock answer");
    require(call(seeker, nullptr) == 14000.0f && stockCalls == 2, "null object did not pass through");
    seeker.parent = stock9X;
    require(call(seeker, &flareObject) == 14000.0f && stockCalls == 3, "stock seeker did not get the stock answer");
    require(hook.restore() && table[seekerLockDistanceSlot / 8] == reinterpret_cast<uintptr_t>(&stock_distance),
        "slot not restored");
    // In flight the same rule applies with the missile's own type as the parent.
    require(rejectDecoy(blockII, project, flare) && !rejectDecoy(stock9X, project, flare)
        && !rejectDecoy(blockII, project, fighter), "in-flight identity");
    // The type accessor must be the reviewed two instructions.
    const unsigned char accessor[8] = {0x48, 0x8d, 0x81, 0xfe, 0x04, 0x00, 0x00, 0xc3};
    const unsigned char otherRegister[8] = {0x48, 0x8d, 0x82, 0xfe, 0x04, 0x00, 0x00, 0xc3};
    const unsigned char noReturn[8] = {0x48, 0x8d, 0x81, 0xfe, 0x04, 0x00, 0x00, 0x90};
    const unsigned char tooFar[8] = {0x48, 0x8d, 0x81, 0x00, 0x00, 0x01, 0x00, 0xc3};
    require(typeAccessorOffset(accessor) == 0x4fe, "type accessor offset");
    require(!typeAccessorOffset(otherRegister) && !typeAccessorOffset(noReturn) && !typeAccessorOffset(tooFar)
        && !typeAccessorOffset(nullptr), "unknown type accessor accepted");
    std::puts("PASS: Block II flare rejection before launch and in flight, stock pass-through and slot restore");
    return 0;
}
