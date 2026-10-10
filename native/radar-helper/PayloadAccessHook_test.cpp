// SPDX-License-Identifier: MIT
#include "PayloadAccessHook.h"
#include <cstdio>
extern "C" __declspec(dllexport) int fixture_payload(int value) { return value + 7; }
static int hooked_payload(int value) { return fixture_payload(value) + 100; }
static int failures = 0;
static void check(bool value, const char* label) {
    if (!value) { std::printf("FAIL %s\n", label); ++failures; }
}
int main() {
    const auto module = GetModuleHandleW(nullptr);
    using Accessor = int(*)(int);
    const auto originalAddress = GetProcAddress(module, "fixture_payload");
    Accessor original{}; std::memcpy(&original, &originalAddress, sizeof(original));
    f23radar::PayloadAccessHook wrong, hook;
    check(original && original(5) == 12, "original export callable");
    check(!wrong.prepare(module, "fixture_payload", reinterpret_cast<void*>(&hooked_payload),
        reinterpret_cast<void*>(&hooked_payload)), "unexpected original refused");
    check(hook.prepare(module, "fixture_payload", reinterpret_cast<void*>(original),
        reinterpret_cast<void*>(&hooked_payload)), "matching export prepared");
    check(hook.install() && hook.install(), "installation is idempotent");
    const auto boundAddress = GetProcAddress(module, "fixture_payload");
    Accessor bound{}; std::memcpy(&bound, &boundAddress, sizeof(bound));
    check(reinterpret_cast<void*>(bound) == hook.address() && bound(5) == 112, "future binding reaches hook");
    check(original(5) == 12, "existing binding remains original");
    check(hook.restore() && hook.restore(), "restoration is idempotent");
    check(GetProcAddress(module, "fixture_payload") == originalAddress, "export restored");
    check(bound(5) == 112, "bound relay remains callable after restoration");
    check(hook.install() && hook.restore(), "reinstallation works");
    if (!failures) std::puts("PASS: early payload export binding, passthrough, rejection and restoration");
    return failures ? 1 : 0;
}
