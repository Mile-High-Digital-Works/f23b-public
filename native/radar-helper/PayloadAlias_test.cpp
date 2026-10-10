// SPDX-License-Identifier: MIT
#include "PayloadAlias.h"
#include <cstdio>
#include <cwchar>

static int failures = 0;
static void check(bool condition, const char* name) {
    if (!condition) { std::printf("FAIL %s\n", name); ++failures; }
}

int main() {
    using namespace f23radar;
    const AliasPair pairs[2] = {{474, stockAim120B}, {475, stockAim9X}};
    WsType malice{4, 4, 7, 0xcd, 474, 0xabcd};
    WsType blockII{4, 4, 7, 0, 475, 0};
    WsType aim120c{4, 4, 7, 0, 106, 0};
    WsType bomb{4, 5, 7, 0, 474, 0};
    WsType container{4, 4, 32, 0, 474, 0};

    for (auto name : {"weapons.missiles.F23B_AIM424_MALICE", "weapons.missiles.F23B_AIM9X_BLOCKII"})
        check(projectWeaponKind(name, std::strlen(name)) > 0, "exact independent identity recognized");
    for (auto name : {"weapons.missiles.AIM_9X", "weapons.missiles.F23B_AIM9X_BLOCKII_OTHER", "F23B_AIM9X_BLOCKII"})
        check(projectWeaponKind(name, std::strlen(name)) == 0, "stock, prefix collisions and incomplete identities rejected");
    check(projectWeaponKind(nullptr, 0) == 0, "missing identity rejected");

    check(stockFor(malice, pairs, 2) == stockAim120B, "MALICE reports AIM-120B");
    check(stockFor(blockII, pairs, 2) == stockAim9X, "Block II reports AIM-9X");
    check(stockFor(aim120c, pairs, 2) == 0, "stock AIM-120C is untouched");
    check(stockFor(bomb, pairs, 2) == 0, "other categories are untouched");
    check(stockFor(container, pairs, 2) == 0, "containers are untouched");

    WsType b{4, 4, 7, 0, stockAim120B, 0}, x{4, 4, 7, 0, stockAim9X, 0};
    check(projectFor(b, pairs, 2) == 474, "AIM-120B reads MALICE data");
    check(projectFor(x, pairs, 2) == 475, "AIM-9X reads Block II data");
    check(projectFor(aim120c, pairs, 2) == 0, "AIM-120C keeps its own data");

    const AliasPair missing[2] = {{0, stockAim120B}, {475, stockAim9X}};
    check(projectFor(b, missing, 2) == 0, "unresolved project type never aliases");
    WsType zero{4, 4, 7, 0, 0, 0};
    check(stockFor(zero, missing, 2) == 0, "level4 zero never matches an unresolved pair");

    WsType copy = withLevel4(&malice, stockAim120B);
    check(copy.level4 == stockAim120B && copy.pad == 0xcd && copy.tail == 0xabcd, "padding preserved");

    EdString shortName = edString("AIM_120"), longName = edString("F23B_AIM424_MALICE");
    check(shortName.size == 7 && shortName.capacity == 15 && std::memcmp(shortName.buffer, "AIM_120", 7) == 0,
          "short name uses the inline buffer");
    check(longName.size == 18 && longName.capacity == 18 && std::strcmp(longName.pointer, "F23B_AIM424_MALICE") == 0,
          "long name uses the pointer");

    // Only code inside the Hornet image sees the alias.
    check(insideImage(0x1000, 0x1000, 0x100), "image start is inside");
    check(insideImage(0x10ff, 0x1000, 0x100), "image last byte is inside");
    check(!insideImage(0x1100, 0x1000, 0x100), "one past the image is outside");
    check(!insideImage(0xfff, 0x1000, 0x100), "below the image is outside");
    check(!insideImage(0x1000, 0, 0x100), "an unloaded image (base 0) contains nothing");

    // The loader reports paths in another case, with either slash, and sometimes the extended prefix.
    const wchar_t* expected = L"d:\\eagle dynamics\\dcs world\\mods\\aircraft\\fa-18c\\bin\\fa18c.dll";
    const wchar_t* loader = L"D:\\Eagle Dynamics\\DCS World\\Mods\\aircraft\\FA-18C\\bin\\FA18C.dll";
    const wchar_t* slashes = L"D:/Eagle Dynamics/DCS World/Mods/aircraft/FA-18C/bin/FA18C.dll";
    const wchar_t* prefixed = L"\\\\?\\D:\\Eagle Dynamics\\DCS World\\Mods\\aircraft\\FA-18C\\bin\\FA18C.dll";
    const wchar_t* other = L"D:\\Eagle Dynamics\\DCS World\\Mods\\aircraft\\F14\\bin\\FA18C.dll";
    check(normalizePath(loader, std::wcslen(loader)) == expected, "loader path is normalized");
    check(normalizePath(slashes, std::wcslen(slashes)) == expected, "forward slashes are normalized");
    check(normalizePath(prefixed, std::wcslen(prefixed)) == expected, "extended-length prefix is removed");
    check(normalizePath(other, std::wcslen(other)) != expected, "another module path does not match");
    check(normalizePath(L"", 0).empty(), "empty path stays empty");
    for (auto path : {loader, slashes, prefixed})
        check(loaderPathEquals(path, std::wcslen(path), expected, std::wcslen(expected)), "allocation-free loader path comparison");
    check(!loaderPathEquals(other, std::wcslen(other), expected, std::wcslen(expected)), "loader rejects another path");

    if (!failures) std::printf("PayloadAlias tests PASS\n");
    return failures ? 1 : 0;
}
