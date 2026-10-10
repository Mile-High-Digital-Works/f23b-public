// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <cstring>

// MSVC x64 RTTI: find the actual MovingObject base of a Registered pointer.
// Never assume that the registered interface starts at the complete object.
// The caller supplies checked memory access and exact native version guards.
template<class Readable>
void* moving_object(void* registered, Readable readable) {
    auto u32 = [](const void* p, size_t n) { uint32_t x; std::memcpy(&x, static_cast<const char*>(p)+n, 4); return x; };
    auto ptr = [](const void* p, size_t n) { uintptr_t x; std::memcpy(&x, static_cast<const char*>(p)+n, 8); return x; };
    if (!readable(registered, 8)) return nullptr;
    auto vt = ptr(registered, 0);
    if (vt < 8 || !readable(reinterpret_cast<void*>(vt-8), 8)) return nullptr;
    auto locator = reinterpret_cast<const char*>(ptr(reinterpret_cast<void*>(vt-8), 0));
    if (!readable(locator, 24) || u32(locator,0) != 1 || u32(locator,8) != 0) return nullptr;
    const auto base = reinterpret_cast<uintptr_t>(locator) - u32(locator,20);
    const auto complete = reinterpret_cast<uintptr_t>(registered) - u32(locator,4);
    auto hierarchy = reinterpret_cast<const char*>(base + u32(locator,16));
    if (!readable(hierarchy,16)) return nullptr;
    auto count = u32(hierarchy,8);
    if (!count || count > 128) return nullptr;
    auto array = reinterpret_cast<const char*>(base + u32(hierarchy,12));
    if (!readable(array, count*4)) return nullptr;
    for (uint32_t i=0; i<count; ++i) {
        auto desc = reinterpret_cast<const char*>(base + u32(array,i*4));
        if (!readable(desc,24)) return nullptr;
        auto type = reinterpret_cast<const char*>(base + u32(desc,0));
        constexpr char expected[] = ".?AVMovingObject@@";
        if (!readable(type,16+sizeof(expected))) return nullptr;
        if (std::memcmp(type+16,expected,sizeof(expected))) continue;
        // Only a fixed, public, unambiguous base is supported.
        if (u32(desc,12) != UINT32_MAX || (u32(desc,20)&0x1f) != 0) return nullptr;
        auto object = reinterpret_cast<void*>(complete + static_cast<int32_t>(u32(desc,8)));
        return readable(object,8) ? object : nullptr;
    }
    return nullptr;
}
