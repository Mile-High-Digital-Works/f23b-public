// SPDX-License-Identifier: MIT
// Offline checks for the MALICE long-flight profile. Weapon-database objects are fixtures.
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "MissileProfile.h"

static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #x); ++failures; } } while (0)

static bool access(const void* p, size_t n, bool) {
    auto current = reinterpret_cast<uintptr_t>(p), end = current+n;
    if (!current || end < current) return false;
    while (current < end) {
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(reinterpret_cast<void*>(current), &info, sizeof(info))
            || info.State != MEM_COMMIT || (info.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
        current = reinterpret_cast<uintptr_t>(info.BaseAddress)+info.RegionSize;
    }
    return true;
}
template<class T> static void put(void* p, size_t o, T v) { std::memcpy(static_cast<char*>(p)+o, &v, sizeof(v)); }
template<class T> static T get(const void* p, size_t o) { T v; std::memcpy(&v, static_cast<const char*>(p)+o, sizeof(v)); return v; }

// Descriptor with an MSVC small std::string name at +8.
static std::vector<char> descriptor(const char* name) {
    std::vector<char> d(0x300);
    std::memcpy(d.data()+8, name, std::strlen(name));
    put<size_t>(d.data()+8, 16, std::strlen(name));
    put<size_t>(d.data()+8, 24, 15);
    return d;
}
struct Weapon {
    std::vector<char> ammo = std::vector<char>(0xd00);
    std::vector<void*> scheme;
    void link(size_t offset = 0xd8) {
        put<uintptr_t>(ammo.data(), offset, reinterpret_cast<uintptr_t>(scheme.data()));
        put<uintptr_t>(ammo.data(), offset+8, reinterpret_cast<uintptr_t>(scheme.data()+scheme.size()));
    }
};

int main() {
    const uintptr_t base = 0x10000000;   // fake WeaponBlocks base for guidance-descriptor identity
    auto guidance = [&](double handoff) {
        std::vector<char> d(0x300);
        put<uintptr_t>(d.data(), 0, base + f23radar::guidanceDescriptorVtable);
        put<double>(d.data(), 0x60, handoff);
        return d;
    };
    auto seeker = [&](double opTime, double fov) {
        auto d = descriptor("sensor");
        put<double>(d.data(), 0x60, opTime); put<double>(d.data(), 0x70, fov);
        put<double>(d.data(), 0xd0, f23radar::stockLockRange5); put<double>(d.data(), 0x100, f23radar::stockDopplerBandwidth);
        return d;
    };
    auto sensor = seeker(600.0, f23radar::stockMaliceFov), ins = descriptor("INS"), other = descriptor("gimbal");
    auto gp = guidance(f23radar::stockHandoff);
    put<double>(ins.data(), 0x60, 100.0);
    put<double>(ins.data(), 0xa0, 3.9);
    Weapon malice; malice.scheme = {other.data(), sensor.data(), gp.data(), ins.data()}; malice.link();

    auto stockSensor = seeker(100.0, f23radar::maliceFov), stockIns = descriptor("INS");
    auto stockGp = guidance(f23radar::stockHandoff);
    put<double>(stockIns.data(), 0x60, 100.0);
    put<double>(stockIns.data(), 0xa0, 3.9);
    Weapon stock; stock.scheme = {stockSensor.data(), stockGp.data(), stockIns.data()}; stock.link();

    // Block lookup by exact name; the unnamed guidance block by descriptor vtable.
    CHECK(f23radar::schemeBlock(malice.ammo.data(), 0xd8, "INS", access) == ins.data());
    CHECK(f23radar::schemeBlock(malice.ammo.data(), 0xd8, "IN", access) == nullptr);
    CHECK(f23radar::schemeBlock(malice.ammo.data(), 0xcc8, "INS", access) == nullptr);
    CHECK(f23radar::guidanceBlock(malice.ammo.data(), 0xd8, base, access) == gp.data());
    CHECK(f23radar::guidanceBlock(malice.ammo.data(), 0xd8, base + 8, access) == nullptr);

    // Plan, apply, restore; the stock AIM-120C is untouched.
    f23radar::ProfilePatch patches[f23radar::profilePatchCapacity]; int count = 0;
    CHECK(f23radar::planMaliceProfile(malice.ammo.data(), stock.ammo.data(), base, patches, count, access) == 1 && count == 6);
    f23radar::applyProfile(patches, count, true);
    CHECK(get<double>(ins.data(), 0xa0) == 60.0 && get<double>(ins.data(), 0x60) == 600.0);
    CHECK(get<double>(sensor.data(), 0x70) == f23radar::maliceFov);
    CHECK(get<double>(gp.data(), 0x60) == 29632.0);
    CHECK(get<double>(sensor.data(), 0xd0) == 40000.0 && get<double>(sensor.data(), 0x100) == 4.0);
    CHECK(get<double>(stockIns.data(), 0xa0) == 3.9 && get<double>(stockIns.data(), 0x60) == 100.0);
    CHECK(get<double>(stockSensor.data(), 0x70) == f23radar::maliceFov);
    CHECK(get<double>(stockGp.data(), 0x60) == 16000.0 && get<double>(stockSensor.data(), 0xd0) == 18000.0
          && get<double>(stockSensor.data(), 0x100) == 16.0);
    f23radar::applyProfile(patches, count, false);
    CHECK(get<double>(ins.data(), 0xa0) == 3.9 && get<double>(ins.data(), 0x60) == 100.0);
    CHECK(get<double>(sensor.data(), 0x70) == f23radar::stockMaliceFov);
    CHECK(get<double>(gp.data(), 0x60) == 16000.0 && get<double>(sensor.data(), 0xd0) == 18000.0
          && get<double>(sensor.data(), 0x100) == 16.0);
    // Planning again while the profile is applied is accepted and changes nothing.
    f23radar::applyProfile(patches, count, true);
    CHECK(f23radar::planMaliceProfile(malice.ammo.data(), stock.ammo.data(), base, patches, count, access) == 1); // idempotent
    CHECK(get<double>(ins.data(), 0xa0) == 60.0 && get<double>(sensor.data(), 0x70) == f23radar::maliceFov);
    f23radar::applyProfile(patches, count, false);   // stale restore values are the applied ones; put the fixture back
    put<double>(ins.data(), 0xa0, 3.9); put<double>(ins.data(), 0x60, 100.0);
    put<double>(sensor.data(), 0x70, f23radar::stockMaliceFov); put<double>(gp.data(), 0x60, f23radar::stockHandoff);
    put<double>(sensor.data(), 0xd0, f23radar::stockLockRange5); put<double>(sensor.data(), 0x100, f23radar::stockDopplerBandwidth);

    // A definition that already declares the 15-degree FOV is accepted; restore keeps the declared value.
    {
        auto declared = seeker(600.0, f23radar::maliceFov), declaredIns = descriptor("INS");
        auto declaredGp = guidance(f23radar::stockHandoff);
        put<double>(declaredIns.data(), 0x60, 100.0); put<double>(declaredIns.data(), 0xa0, 3.9);
        Weapon narrow; narrow.scheme = {declared.data(), declaredGp.data(), declaredIns.data()}; narrow.link();
        f23radar::ProfilePatch narrowPatches[f23radar::profilePatchCapacity]; int narrowCount = 0;
        CHECK(f23radar::planMaliceProfile(narrow.ammo.data(), stock.ammo.data(), base, narrowPatches, narrowCount, access) == 1);
        f23radar::applyProfile(narrowPatches, narrowCount, true);
        CHECK(get<double>(declared.data(), 0x70) == f23radar::maliceFov);
        f23radar::applyProfile(narrowPatches, narrowCount, false);
        CHECK(get<double>(declared.data(), 0x70) == f23radar::maliceFov);
        CHECK(get<double>(declaredIns.data(), 0xa0) == 3.9 && get<double>(declaredGp.data(), 0x60) == 16000.0);
    }
    // Identity: a descriptor without MALICE's 600 s sensor lifetime is refused.
    CHECK(f23radar::planMaliceProfile(stock.ammo.data(), nullptr, base, patches, count, access) == -2);
    // Exclusivity: a shared INS, seeker or guidance descriptor is refused.
    Weapon shared; shared.scheme = {stockSensor.data(), ins.data()}; shared.link();
    CHECK(f23radar::planMaliceProfile(malice.ammo.data(), shared.ammo.data(), base, patches, count, access) == -4);
    Weapon sharedSensor; sharedSensor.scheme = {sensor.data(), stockIns.data()}; sharedSensor.link();
    CHECK(f23radar::planMaliceProfile(malice.ammo.data(), sharedSensor.ammo.data(), base, patches, count, access) == -7);
    Weapon sharedGuidance; sharedGuidance.scheme = {stockSensor.data(), gp.data(), stockIns.data()}; sharedGuidance.link();
    CHECK(f23radar::planMaliceProfile(malice.ammo.data(), sharedGuidance.ammo.data(), base, patches, count, access) == -12);
    // Unknown current values are refused (another build or another change).
    put<double>(ins.data(), 0xa0, 7.0);
    CHECK(f23radar::planMaliceProfile(malice.ammo.data(), stock.ammo.data(), base, patches, count, access) == -5);
    put<double>(ins.data(), 0xa0, 3.9);
    put<double>(sensor.data(), 0x70, 0.7);
    CHECK(f23radar::planMaliceProfile(malice.ammo.data(), stock.ammo.data(), base, patches, count, access) == -8);
    put<double>(sensor.data(), 0x70, f23radar::stockMaliceFov);
    put<double>(gp.data(), 0x60, 20000.0);
    CHECK(f23radar::planMaliceProfile(malice.ammo.data(), stock.ammo.data(), base, patches, count, access) == -9);
    put<double>(gp.data(), 0x60, f23radar::stockHandoff);
    put<double>(sensor.data(), 0xd0, 30000.0);
    CHECK(f23radar::planMaliceProfile(malice.ammo.data(), stock.ammo.data(), base, patches, count, access) == -10);
    put<double>(sensor.data(), 0xd0, f23radar::stockLockRange5);
    put<double>(sensor.data(), 0x100, 8.0);
    CHECK(f23radar::planMaliceProfile(malice.ammo.data(), stock.ammo.data(), base, patches, count, access) == -11);
    put<double>(sensor.data(), 0x100, f23radar::stockDopplerBandwidth);
    // A scheme without an identifiable guidance block is refused.
    Weapon noGuidance; noGuidance.scheme = {sensor.data(), ins.data()}; noGuidance.link();
    CHECK(f23radar::planMaliceProfile(noGuidance.ammo.data(), stock.ammo.data(), base, patches, count, access) == -13);
    // Network-variant scheme is patched too when present.
    auto netSensor = seeker(600.0, f23radar::stockMaliceFov), netIns = descriptor("INS");
    auto netGp = guidance(f23radar::stockHandoff);
    put<double>(netIns.data(), 0x60, 100.0); put<double>(netIns.data(), 0xa0, 3.9);
    std::vector<void*> netScheme = {netSensor.data(), netGp.data(), netIns.data()};
    put<uintptr_t>(malice.ammo.data(), 0xcc8, reinterpret_cast<uintptr_t>(netScheme.data()));
    put<uintptr_t>(malice.ammo.data(), 0xcd0, reinterpret_cast<uintptr_t>(netScheme.data()+netScheme.size()));
    CHECK(f23radar::planMaliceProfile(malice.ammo.data(), stock.ammo.data(), base, patches, count, access) == 1 && count == 12);
    // Missing weapon.
    CHECK(f23radar::planMaliceProfile(nullptr, stock.ammo.data(), base, patches, count, access) == -1);

    // Map-walk fallback: fake `mov r9,[rip+disp]` + std::map with padded keys.
    {
        std::vector<char> head(0x30), a(0x30), b(0x30), c(0x30);
        put<unsigned char>(head.data(), 0x19, 1);
        auto setNode = [&](std::vector<char>& n, void* left, void* right, uint16_t level4, unsigned char pad, void* value) {
            put<void*>(n.data(), 0, left ? left : head.data()); put<void*>(n.data(), 0x10, right ? right : head.data());
            const unsigned char key[8] = {4,4,7,pad,0,0,pad,pad}; std::memcpy(n.data()+0x20, key, 8);
            put<uint16_t>(n.data(), 0x24, level4); put<void*>(n.data(), 0x28, value);
        };
        setNode(a, nullptr, nullptr, 24, 0xcc, malice.ammo.data());   // padding not zero
        setNode(c, nullptr, nullptr, 106, 0, stock.ammo.data());
        setNode(b, a.data(), c.data(), 7, 0, nullptr);
        put<void*>(head.data(), 8, b.data());
        std::vector<unsigned char> code(16);
        void* holder = head.data(); void** holderSlot = &holder;
        code[0]=0x4c; code[1]=0x8b; code[2]=0x0d;
        const int32_t disp = static_cast<int32_t>(reinterpret_cast<intptr_t>(holderSlot) - reinterpret_cast<intptr_t>(code.data()+7));
        std::memcpy(code.data()+3, &disp, 4);
        const bool reachable = reinterpret_cast<intptr_t>(holderSlot) - reinterpret_cast<intptr_t>(code.data()+7) == disp;
        if (reachable) {
            CHECK(f23radar::findAmmunition(code.data(), 24, access) == malice.ammo.data());
            CHECK(f23radar::findAmmunition(code.data(), 106, access) == stock.ammo.data());
            CHECK(f23radar::findAmmunition(code.data(), 99, access) == nullptr);
        }
        code[2] = 0x05; CHECK(f23radar::findAmmunition(code.data(), 24, access) == nullptr);  // unexpected instruction
    }
    if (failures) return EXIT_FAILURE;
    std::printf("PASS: MALICE profile identity, exclusivity, value checks, handoff/seeker fields, apply/restore and network scheme\n");
    return EXIT_SUCCESS;
}
