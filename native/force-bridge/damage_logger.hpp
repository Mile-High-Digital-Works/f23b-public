// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iomanip>

namespace f23b::bridge::damage {
using Native = void(*)(int,double);
inline void emit_and_forward(double bridge_time,int element,double integrity,Native native) {
    wchar_t path[2048]{};
    const auto length=GetEnvironmentVariableW(L"F23B_FORCE_BRIDGE_DAMAGE",path,2048);
    if(length>0 && length<2048) {
        // Logging failure must never suppress or change the native callback.
        try {
            std::ofstream out(std::filesystem::path(path),std::ios::app);
            out << std::setprecision(17) << bridge_time << ',' << element << ',' << integrity << '\n';
        } catch(...) {}
    }
    native(element,integrity);
}
}
