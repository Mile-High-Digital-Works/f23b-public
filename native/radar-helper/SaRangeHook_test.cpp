// SPDX-License-Identifier: MIT
#include "SaRangeHook.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace f23radar;
static void require(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
static std::vector<unsigned char> stockImage() {
    std::vector<unsigned char> image(0x700000, 0xcc);
    std::memcpy(image.data() + saRangeStockConstant, &saRangeStockLimit, 8);
    std::memcpy(image.data() + saRangeExtendedConstant, &saRangeExtendedLimit, 8);
    for (const auto& p : saRangePatches) {
        std::memcpy(image.data() + p.rva, p.prefix, p.prefixSize);
        std::memcpy(image.data() + p.rva + p.valueOffset, p.stock, p.valueSize);
    }
    return image;
}
int main() {
    auto image = stockImage();
    require(saRangeState(image.data()) == 1, "stock image not recognised");
    saRangeWrite(image.data(), true);
    require(saRangeState(image.data()) == 2, "extended values not recognised");
    // Integer path: mov eax, imm32 now loads 640.
    int32_t wrap{}; std::memcpy(&wrap, image.data() + saRangePatches[0].rva + 1, 4);
    require(wrap == 640, "integer wrap is not 640");
    // Double path: the rip-relative load reaches 640.0 and the movabs immediate is 640.0.
    int32_t disp{}; std::memcpy(&disp, image.data() + saRangePatches[1].rva + 4, 4);
    double loaded{}; std::memcpy(&loaded, image.data() + saRangePatches[1].rva + 8 + disp, 8);
    double stored{}; std::memcpy(&stored, image.data() + saRangePatches[2].rva + 2, 8);
    require(loaded == 640.0 && stored == 640.0, "double path does not wrap to 640");
    // The stock displacement reaches the shared 320.0 constant.
    std::memcpy(&disp, saRangePatches[1].stock, 4);
    require(saRangePatches[1].rva + 8 + disp == saRangeStockConstant, "stock displacement is wrong");
    saRangeWrite(image.data(), false);
    require(saRangeState(image.data()) == 1 && image == stockImage(), "restore changed bytes");
    // Refuse unknown code, a partial patch, or a changed constant.
    auto other = stockImage(); other[saRangePatches[0].rva] = 0x90;
    require(saRangeState(other.data()) == 0, "changed opcode accepted");
    auto partial = stockImage();
    std::memcpy(partial.data() + saRangePatches[2].rva + 2, saRangePatches[2].extended, 8);
    require(saRangeState(partial.data()) == 0, "half-patched image accepted");
    auto constant = stockImage(); const double wrong = 330.0;
    std::memcpy(constant.data() + saRangeStockConstant, &wrong, 8);
    require(saRangeState(constant.data()) == 0, "changed 320 constant accepted");
    std::puts("PASS: SA wrap sites (integer and double paths), 640 patch, exact restore, refusal of unknown bytes");
}
