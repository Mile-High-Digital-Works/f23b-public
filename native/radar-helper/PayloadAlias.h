// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <cstring>
#include <string>

namespace f23radar {
// Independent MALICE and Block II through the native Hornet weapon path (DCS 2.9.30.28738).
// FA18C reads each station's wsType through the human payload interface (IwHumanPayload
// vtable +0x98 station store, +0xa0 weapon inside a container). It then accepts only listed
// stock missiles, for example the Sidewinder set at FA18C RVA 0x31b1d9 in 2.9.29.27468 (level4 22, 136, 143, 1007).
// The Hornet reads these types while its cockpit devices initialize, before any cockpit Lua script
// runs. The F-23B ForceBridge loads the helper before forwarding its first native callback.
// DLL-load notifications only record the image. Ordinary initialization installs the imports;
// the first Hornet payload call installs the payload slots before station enumeration.
// Only calls that come from FA18C code see the alias. It reports MALICE as AIM-120B (24) and
// Block II as AIM-9X (136). FA18C imports that read weapon data by type get the project type back
// while the payload carries that project missile, so launch-zone, seeker and descriptor data stay the
// project's own. AI, other aircraft, other modules and the world simulation see only the real types.
// wsType layout (Weapons.dll map key): level1-3 bytes at +0..+2, level4 uint16 at +4.
struct WsType {
    unsigned char level1, level2, level3, pad;
    uint16_t level4, tail;
};
static_assert(sizeof(WsType) == 8, "wsType key is 8 bytes");

struct AliasPair { uint16_t project, stock; };

// Exact database resource identities. Physical similarity is not an identity test.
inline int projectWeaponKind(const char* text, size_t length) {
    if (!text) return 0;
    constexpr const char* names[] = {"weapons.missiles.F23B_AIM424_MALICE", "weapons.missiles.F23B_AIM9X_BLOCKII"};
    for (int i = 0; i < 2; ++i)
        if (length == std::strlen(names[i]) && std::memcmp(text, names[i], length) == 0) return i + 1;
    return 0;
}

constexpr uint16_t stockAim120B = 24, stockAim9X = 136;
constexpr uintptr_t payloadStationTypeSlot = 0x98, payloadContainerTypeSlot = 0xa0;
constexpr uintptr_t payloadStationCountSlot = 0x1b8, payloadWeaponCountSlot = 0xd0;
// FA18C import-table slots (2.9.30.28738, loaded image) that consume a wsType.
constexpr uintptr_t importSetMissileData = 0x60ff20;      // cockpit::MissileSight::setMissileData
constexpr uintptr_t importDescriptorByType = 0x611520;    // wGetAmmunitionDescriptorByType
constexpr uintptr_t importSidewinderNew = 0x610380;       // cockpit::eqSidewinderNew::eqSidewinderNew
constexpr uintptr_t importCurrentPayload = 0x60fe58;      // cockpit::c_payload, verified against live export
constexpr uintptr_t importLaunchZone = 0x611480;           // Weapons.dll DLZ
constexpr uintptr_t armamentVtable = 0x61ebb8, amraamSelectorVtable = 0x61e7c8;

// The native radar-missile launch at 0x2f3a1d uses SMS+0xda14 to select a
// station selector at +0xded0. AMRAAM is index 2 (constructor 0x2ec2e4).
// Its getter at 0x2e6b38 reads a signed index at +0x20 into the pair vector
// at +8. Read the same pair without advancing the native selector.
template<class Access>
int amraamStation(const void* sms, const void* context, uintptr_t base, Access access) {
    if (!base || !context || !access(sms, 0xdee8, false)) return -1;
    const auto read = [](const void* object, size_t offset, auto& value) {
        std::memcpy(&value, static_cast<const unsigned char*>(object) + offset, sizeof(value));
    };
    uintptr_t table = 0; const void* ownContext = nullptr; int mode = -1;
    const void* selector = nullptr;
    read(sms, 0, table); read(sms, 0x18, ownContext); read(sms, 0xda14, mode);
    if (table != base + armamentVtable || ownContext != context || mode != 2) return -1;
    read(sms, 0xdee0, selector);
    if (!access(selector, 0x21, false)) return -1;
    read(selector, 0, table);
    uintptr_t first = 0, end = 0; signed char index = -1;
    read(selector, 8, first); read(selector, 0x10, end); read(selector, 0x20, index);
    if (table != base + amraamSelectorVtable || index < 0 || !first || end < first
        || (end - first) % 2 || (end - first) / 2 > 64
        || unsigned(index) >= (end - first) / 2
        || !access(reinterpret_cast<void*>(first), end - first, false)) return -1;
    signed char pair[2]{};
    std::memcpy(pair, reinterpret_cast<void*>(first + 2 * index), sizeof(pair));
    return pair[0] >= 0 && pair[0] < 64 && pair[1] > 0 ? pair[0] : -1;
}

// Carried weapons alone cannot select a launch zone: use the current station.
inline void* launchZoneConstant(void* input, void* stock, void* project,
                               const WsType& selected, uint16_t malice, bool owner) {
    return owner && malice && selected.level1 == 4 && selected.level2 == 4
        && selected.level3 == 7 && selected.level4 == malice
        && stock && project && project != stock && input == stock ? project : input;
}

// True when the address lies inside the image [base, base + size).
inline bool insideImage(uintptr_t address, uintptr_t base, size_t size) {
    return base && address >= base && address - base < size;
}

// Loader paths differ in case, slash style and the \\?\ prefix. Compare the normalized form.
inline std::wstring normalizePath(const wchar_t* text, size_t length) {
    std::wstring out(text, length);
    if (out.compare(0, 4, L"\\\\?\\") == 0) out.erase(0, 4);
    for (auto& character : out) {
        if (character == L'/') character = L'\\';
        else if (character >= L'A' && character <= L'Z') character = static_cast<wchar_t>(character - L'A' + L'a');
    }
    return out;
}

// Notification callbacks cannot allocate or call another DLL. Compare against
// the normalized path prepared during ordinary initialization.
inline bool loaderPathEquals(const wchar_t* text, size_t length, const wchar_t* expected, size_t count) {
    size_t start = 0;
    if (length >= 4 && text[0] == L'\\' && text[1] == L'\\' && text[2] == L'?' && text[3] == L'\\') start = 4;
    if (length - start != count) return false;
    for (size_t i = 0; i < count; ++i) {
        auto c = text[start + i];
        if (c == L'/') c = L'\\';
        if (c >= L'A' && c <= L'Z') c = static_cast<wchar_t>(c - L'A' + L'a');
        if (c != expected[i]) return false;
    }
    return true;
}

inline bool airToAirMissile(const WsType& type) {
    return type.level1 == 4 && type.level2 == 4 && type.level3 == 7;
}

// Project level4 -> stock level4, or 0 when the type is not a project missile.
inline uint16_t stockFor(const WsType& type, const AliasPair* pairs, int count) {
    if (!airToAirMissile(type)) return 0;
    for (int i = 0; i < count; ++i)
        if (pairs[i].project && type.level4 == pairs[i].project) return pairs[i].stock;
    return 0;
}

// Stock level4 -> project level4, or 0 when the type is not an aliased stock missile.
inline uint16_t projectFor(const WsType& type, const AliasPair* pairs, int count) {
    if (!airToAirMissile(type)) return 0;
    for (int i = 0; i < count; ++i)
        if (pairs[i].project && type.level4 == pairs[i].stock) return pairs[i].project;
    return 0;
}

// Copy a type and replace its level4. Padding bytes are preserved.
inline WsType withLevel4(const void* type, uint16_t level4) {
    WsType out;
    std::memcpy(&out, type, sizeof(out));
    out.level4 = level4;
    return out;
}

// MSVC/ED basic_string<char> layout: 16-byte buffer or pointer, size, capacity.
struct EdString {
    union { char buffer[16]; const char* pointer; };
    size_t size, capacity;
};
static_assert(sizeof(EdString) == 32, "ed::basic_string<char> is 32 bytes");

inline EdString edString(const char* text) {
    EdString out{};
    out.size = std::strlen(text);
    if (out.size <= 15) { std::memcpy(out.buffer, text, out.size); out.capacity = 15; }
    else { out.pointer = text; out.capacity = out.size; }
    return out;
}
}
