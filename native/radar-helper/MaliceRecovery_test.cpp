// SPDX-License-Identifier: MIT
#include "MaliceRecovery.h"
#include "MaliceLoftHook.h"
#include <cstdio>
#include <limits>
#include <vector>
#include <iostream>
using namespace f23radar;
static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL %d: %s\n", __LINE__, #x); ++failures; } } while (0)
template<class T> static void put(void* p, size_t offset, T value) { std::memcpy(static_cast<char*>(p)+offset, &value, sizeof(value)); }
static bool access(const void* p, size_t, bool) { return p != nullptr; }
static void* liveGP;
static bool writerCalled;
static void writer(void* p, double step) {
    writerCalled = true;
    CHECK(p != liveGP && profileField<int>(liveGP, 0x158) == 2);
    CHECK(profileField<int>(p, 0x158) == 1 && step == 0.005);
    CHECK(profileField<void*>(p, 8) == profileField<void*>(liveGP, 8));
    auto outputs = profileField<double*>(profileField<void*>(p, 0x10), 8);
    outputs[0] = profileField<double>(p, 0x180);
    put(p, 0x160, static_cast<unsigned char>(0));
}
static void replacement(void* p, double step) { *static_cast<double*>(p) = step; }

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "--replay") == 0) {
        int malice, mode, signal, sensor; unsigned target; double age;
        while (std::cin >> malice >> mode >> signal >> sensor >> target >> age)
            std::cout << useMaliceRecovery(malice != 0, mode, signal != 0, sensor, target, age) << '\n';
        return 0;
    }
    CHECK(rejectMaliceChaff(true, 0x100, 13));
    CHECK(!rejectMaliceChaff(false, 0x100, 13));
    CHECK(!rejectMaliceChaff(true, 0x1, 13));
    CHECK(!rejectMaliceChaff(true, 0x100, 14));
    CHECK(useMaliceRecovery(true, 2, false, 0, 0, 1.365));
    CHECK(!useMaliceRecovery(false, 2, false, 0, 0, 1));
    CHECK(!useMaliceRecovery(true, 1, false, 0, 0, 1));
    CHECK(!useMaliceRecovery(true, 2, true, 0, 0, 1));
    CHECK(!useMaliceRecovery(true, 2, false, 3, 0, 1));
    CHECK(!useMaliceRecovery(true, 2, false, 0, 42, 1));
    CHECK(!useMaliceRecovery(true, 2, false, 0, 0, 4.5));
    CHECK(!useMaliceRecovery(true, 2, false, 0, 0, -1));
    CHECK(!useMaliceRecovery(true, 2, false, 0, 0, std::numeric_limits<double>::quiet_NaN()));
    CHECK(!useMaliceRecovery(true, 2, false, 0, 0, std::numeric_limits<double>::infinity()));
    CHECK(useMaliceRecovery(true, 2, false, 0, 0, 4.499));
    CHECK(useMaliceRecovery(true, 2, false, 0, 0, 0));
    // Brief memory in the successful bomber shot must retain terminal guidance.
    CHECK(!useMaliceRecovery(true, 2, false, 3, 0, 0.195));

    // A failed missile must not disable another missile or alter a startup failure.
    int startupStatus = 1;
    RecoveryRecord first, second;
    first.observe(10, false); second.observe(12, false);
    CHECK(first.enabled(startupStatus) && second.enabled(startupStatus));
    unsigned skipped = 0;
    if (first.fail()) ++skipped;
    CHECK(first.failed && !first.enabled(startupStatus) && startupStatus == 1);
    if (first.fail()) ++skipped;
    CHECK(skipped == 1); // one error and one counted missile, not one per frame
    first.observe(11, true); CHECK(first.failed); // reacquisition does not clear a failed flight
    CHECK(useMaliceRecovery(second.enabled(startupStatus), 2, false, 0, 0, 1.365));
    first.observe(.1, false);
    CHECK(!first.failed && !first.running && first.enabled(startupStatus)); // reused system
    CHECK(!first.enabled(-53) && !second.enabled(-54) && !second.enabled(-55));

    std::array<unsigned char, 0x4f8> gp{};
    std::array<unsigned char, 32> system{}, desc{};
    double output[4]{}; liveGP = gp.data();
    put(gp.data(), 8, static_cast<void*>(desc.data())); put(gp.data(), 0x10, static_cast<void*>(system.data()));
    put(system.data(), 8, output); put(gp.data(), 0x158, 2); put(gp.data(), 0x180, 1400.0);
    put(gp.data(), 0x160, static_cast<unsigned char>(1));
    CHECK(!writeInertialReferences(gp.data(), .005, writer, access) && !writerCalled);
    put(gp.data(), 0x163, static_cast<unsigned char>(1)); const auto before = gp;
    CHECK(writeInertialReferences(gp.data(), .005, writer, access));
    CHECK(writerCalled && output[0] == 1400 && gp == before); // live mode and fields are untouched

    // Execute the new eight-byte entry, its trampoline, and exact restore in this process only.
    auto code = static_cast<unsigned char*>(VirtualAlloc(nullptr, 4096, MEM_RESERVE|MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    CHECK(code);
    if (code) {
        const unsigned char entry[] = {0x48,0x8b,0xc4,0x48,0x89,0x58,0x18,0x55};
        const unsigned char tail[] = {0x5d,0x48,0x8b,0x5c,0x24,0x18,0xc3};
        std::memcpy(code, entry, sizeof(entry)); std::memcpy(code+8, tail, sizeof(tail));
        FlushInstructionCache(GetCurrentProcess(), code, 32);
        AlignedEntryHook hook, bad;
        code[7] = 0x56;
        CHECK(!bad.prepare(reinterpret_cast<uintptr_t>(code), reinterpret_cast<void*>(&replacement), entry, sizeof(entry)));
        code[7] = 0x55;
        CHECK(hook.prepare(reinterpret_cast<uintptr_t>(code), reinterpret_cast<void*>(&replacement), entry, sizeof(entry)));
        double result = 0;
        auto original = reinterpret_cast<GuidanceStep>(hook.trampoline());
        original(&result, 7); CHECK(result == 0);
        CHECK(hook.install() && hook.installed());
        reinterpret_cast<GuidanceStep>(code)(&result, 7); CHECK(result == 7);
        original(&result, 9); CHECK(result == 7);
        CHECK(hook.restore() && !hook.installed() && !std::memcmp(code, entry, sizeof(entry)));
        reinterpret_cast<GuidanceStep>(code)(&result, 9); CHECK(result == 7);
        CHECK(hook.install() && hook.restore());
        VirtualFree(code, 0, MEM_RELEASE);
    }
    std::printf("MALICE recovery tests: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
