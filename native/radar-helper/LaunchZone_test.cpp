// SPDX-License-Identifier: MIT
#include "PayloadAlias.h"
#include "SupportHook.h"
#include <cstdio>

static int failures = 0;
static void check(bool condition, const char* name) {
    if (!condition) { std::printf("FAIL %s\n", name); ++failures; }
}
static void stockFunction() {}
static void replacementFunction() {}
static void foreignFunction() {}

int main() {
    using namespace f23radar;
    unsigned char sms[0xdf00]{}, selector[0x68]{};
    signed char stations[] = {2, 1, 3, 1, 5, 1};
    int context = 0, otherContext = 0;
    const uintptr_t base = 0x10000000;
    auto set = [](void* object, size_t offset, auto value) {
        std::memcpy(static_cast<unsigned char*>(object) + offset, &value, sizeof(value));
    };
    set(sms, 0, base + armamentVtable); set(sms, 0x18, &context); set(sms, 0xda14, 2);
    set(sms, 0xdee0, selector + 0);
    set(selector, 0, base + amraamSelectorVtable);
    set(selector, 8, stations + 0); set(selector, 0x10, stations + sizeof(stations));
    set(selector, 0x20, static_cast<signed char>(1));
    auto access = [&](const void* p, size_t n, bool) {
        auto at = reinterpret_cast<uintptr_t>(p);
        for (auto region : {std::pair<const void*, size_t>{sms, sizeof(sms)},
                           {selector, sizeof(selector)}, {stations, sizeof(stations)}}) {
            auto begin = reinterpret_cast<uintptr_t>(region.first);
            if (at >= begin && at - begin <= region.second && n <= region.second - (at - begin)) return true;
        }
        return false;
    };
    check(amraamStation(sms, &context, base, access) == 3, "native selected station pair");
    check(amraamStation(sms, &otherContext, base, access) == -1, "other cockpit refused");
    check(amraamStation(sms, &context, base + 8, access) == -1, "unexpected vtable refused");
    set(selector, 0x20, static_cast<signed char>(-1));
    check(amraamStation(sms, &context, base, access) == -1, "cleared selection refused");
    set(selector, 0x20, static_cast<signed char>(3));
    check(amraamStation(sms, &context, base, access) == -1, "one-past-end selection refused");
    set(selector, 0x20, static_cast<signed char>(2));
    check(amraamStation(sms, &context, base, access) == 5, "selection change read without cached station");
    set(sms, 0xda14, 1);
    check(amraamStation(sms, &context, base, access) == -1, "other weapon family refused");
    check(amraamStation(nullptr, &context, base, access) == -1, "missing SMS refused");

    int stock = 1, project = 2, other = 3;
    WsType malice{4,4,7,0,474,0}, stockB{4,4,7,0,24,0}, stockC{4,4,7,0,106,0}, ir{4,4,7,0,475,0};
    for (const auto& selected : {stockB, stockC, ir, WsType{}})
        check(launchZoneConstant(&stock, &stock, &project, selected, 474, true) == &stock,
              "stock/IR/unknown selection stays stock even when MALICE is carried");
    check(launchZoneConstant(&stock, &stock, &project, malice, 474, true) == &project, "selected MALICE substitutes");
    check(launchZoneConstant(&other, &stock, &project, malice, 474, true) == &other, "other input constant passes through");
    check(launchZoneConstant(&stock, &stock, nullptr, malice, 474, true) == &stock, "null project passes through");
    check(launchZoneConstant(&stock, &stock, &stock, malice, 474, true) == &stock, "shared constant passes through");
    check(launchZoneConstant(&stock, &stock, &project, malice, 474, false) == &stock, "other cockpit/stale payload passes through");
    check(launchZoneConstant(&stock, &stock, &project, malice, 0, true) == &stock, "unresolved identity passes through");

    alignas(8) uintptr_t slot = reinterpret_cast<uintptr_t>(&stockFunction);
    const auto original = slot;
    SlotHook hook;
    check(hook.prepare(&slot, original, reinterpret_cast<void*>(&replacementFunction)), "exact import accepted");
    check(hook.install() && slot == reinterpret_cast<uintptr_t>(&replacementFunction), "in-memory import installed");
    check(hook.restore() && slot == original, "import restored byte-for-byte");
    slot = reinterpret_cast<uintptr_t>(&foreignFunction);
    check(!hook.prepare(&slot, original, reinterpret_cast<void*>(&replacementFunction)), "foreign import refused");
    check(!hook.restore() && slot == reinterpret_cast<uintptr_t>(&foreignFunction), "foreign replacement is preserved");
    if (!failures) std::puts("LaunchZone tests PASS: selection, mixed loadouts, null/shared constants, scope, import guard and restoration");
    return failures ? 1 : 0;
}
