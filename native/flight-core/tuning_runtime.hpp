// SPDX-License-Identifier: MIT
#pragma once
#include "fitted_parameters.hpp"
#include "tuning.hpp"
namespace f23b::flight_core {
inline Parameters runtime_parameters=fitted_parameters();
inline DryThrustSettings runtime_dry_thrust;
inline void load_development_tuning(const std::filesystem::path& profile) noexcept {
    runtime_parameters=fitted_parameters();
    runtime_dry_thrust={};
    try {
        std::vector<std::string> log;
        tuning::load(profile/"Config"/"F23B-tuning.json",runtime_parameters,log,&runtime_dry_thrust);
        std::ofstream file(profile/"Logs"/"F23B-tuning.log",std::ios::app);
        file<<"F23B tuning: aircraft start\n";
        for(const auto& line:log)file<<line<<'\n';
    } catch(...) { runtime_parameters=fitted_parameters();runtime_dry_thrust={}; }
}
}
