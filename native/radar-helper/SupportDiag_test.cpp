// SPDX-License-Identifier: MIT
// Offline checks for the read-only missile-support diagnostics. Native objects are fixtures.
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "SupportDiag.h"

static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #x); ++failures; } } while (0)
static bool access(const void* p, size_t n, bool) { return p && n; }
template<class T> static void put(void* p, size_t offset, T value) { std::memcpy(static_cast<char*>(p)+offset, &value, sizeof(value)); }

int main() {
    std::vector<char> radar(0x100), mc(0x4040), link(0x40), tracks(2*f23radar::msiTrackStride);
    put<void*>(mc.data(), 0x3d30, link.data());
    put<unsigned char>(link.data(), 0x38, 1);
    put<void*>(link.data(), 0x30, radar.data());
    put<uintptr_t>(mc.data(), 0x4028, reinterpret_cast<uintptr_t>(tracks.data()));
    put<uintptr_t>(mc.data(), 0x4030, reinterpret_cast<uintptr_t>(tracks.data()+tracks.size()));
    char* mirage = tracks.data()+f23radar::msiTrackStride;
    put<unsigned>(mirage, 8, 16798720u);
    put<double>(mirage, 0xe8, 0.0); put<double>(mirage, 0xf0, 590.0); put<double>(mirage, 0xf8, 0.0);
    put<unsigned char>(mirage, 0x101, 0); put<unsigned char>(mirage, 0x19d, 1); put<uint32_t>(mirage, 0x198, 0x10u);

    // Record lookup matches the support test's trackfile rule.
    CHECK(f23radar::msiRecord(mc.data(), radar.data(), 16798720u, access) == mirage);
    CHECK(f23radar::msiRecord(mc.data(), radar.data(), 1u, access) == nullptr);
    CHECK(f23radar::msiRecord(mc.data(), nullptr, 16798720u, access) == nullptr);
    auto snap = f23radar::trackSnapshot(mirage);
    CHECK(snap.found && snap.report[1] == 590.0 && snap.memory == 1 && snap.fresh == 0 && snap.mask == 0x10u);
    CHECK(!f23radar::trackSnapshot(nullptr).found);

    // Lines: immediate on first note and on decision change; otherwise every 2 s; silent after 5 s unused.
    f23radar::SupportDiagTable table; char line[256];
    table.note(16798720u, f23radar::ExtensionSupport, snap, 600.0);
    CHECK(table.nextLine(600.0, line, sizeof(line)));
    CHECK(std::string(line).find("decision=extension") != std::string::npos
          && std::string(line).find("memory=1") != std::string::npos
          && std::string(line).find("age_f0=10.0") != std::string::npos
          && std::string(line).find("age_e8=-1.0") != std::string::npos);
    CHECK(!table.nextLine(600.5, line, sizeof(line)));
    table.note(16798720u, f23radar::ExtensionSupport, snap, 601.0);
    CHECK(!table.nextLine(601.0, line, sizeof(line)));
    table.note(16798720u, f23radar::ExtensionSupport, snap, 602.1);
    CHECK(table.nextLine(602.1, line, sizeof(line)));                      // periodic
    table.note(16798720u, f23radar::NoTrackfile, f23radar::TrackSnapshot{}, 602.3);
    CHECK(table.nextLine(602.3, line, sizeof(line)) && std::string(line).find("decision=no-trackfile trackfile=0") != std::string::npos);
    CHECK(!table.nextLine(610.0, line, sizeof(line)));                    // unused for more than 5 s
    // Nine targets: the table keeps eight and ignores the rest without error.
    for (unsigned i = 1; i <= 9; ++i) table.note(i, f23radar::NativeSupport, snap, 700.0);
    int lines = 0; while (table.nextLine(700.0, line, sizeof(line))) ++lines;
    CHECK(lines == 8);
    if (failures) return EXIT_FAILURE;
    std::printf("PASS: support diagnostics record lookup, snapshot, change/periodic lines and capacity\n");
    return EXIT_SUCCESS;
}
