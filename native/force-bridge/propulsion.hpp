// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <f23b/performance/performance_model.hpp>
#include "../flight-core/development_options.hpp"

namespace f23b::bridge {
// Owner-requested moderate speed tune; simulation settings, not F-22 calibration.
inline constexpr double native_thrust_scale = 2.0; // previously 1.75 (+14.29%)
// Native SDK fan-speed fraction, not cockpit core RPM or a sampled HOTAS axis.
// Measured hot-start idle is 0.212 steady / below 0.269 during settling.
// Keep that range unaugmented; recover the accepted gain before MIL power.
inline double native_engine_gain(double related_rpm) {
    if (!std::isfinite(related_rpm)) return 1.0;
    const double blend = std::clamp((related_rpm - 0.30) / 0.40, 0.0, 1.0);
    return 1.0 + (native_thrust_scale - 1.0) * blend * blend * (3.0 - 2.0 * blend);
}
inline double development_engine_gain(double related_rpm,double mach,bool augmented,const DryThrustSettings& settings) {
    const double original=native_engine_gain(related_rpm);
    if(!settings.enabled||augmented||!std::isfinite(mach)||mach<=settings.start_mach)return original;
    const double blend=std::clamp((mach-settings.start_mach)/(settings.end_mach-settings.start_mach),0.,1.);
    const double smooth=blend*blend*(3.-2.*blend);
    // Delay most of the reduction until above the transonic drag rise;
    // otherwise a second, lower dry equilibrium prevents reaching Mach 2.
    const double powered_gain=2.+(settings.end_gain-2.)*smooth*smooth*smooth;
    // Keep the idle correction and native spool response; only powered gain changes.
    return 1.+(original-1.)*(powered_gain-1.);
}
inline constexpr performance::Options flight_options{
    .high_speed_zero_lift_drag_reduction = 0.08,
};

inline double add_native_engine(performance::Output& output,
    const performance::MassState& mass, std::size_t engine,
    double native_thrust_n, double native_flow_kg_s, double related_rpm,
    double mach=0.,bool augmented=false,const DryThrustSettings& settings={}) {
    // Accepted engine-axis locations in VisualConfig.lua. Straight axial thrust;
    // no virtual vectoring or independently running second engine simulation.
    constexpr double axis_y_m = -0.320670754;
    constexpr double half_spacing_m = 1.311020017;
    const bool burning = std::isfinite(native_flow_kg_s) && native_flow_kg_s > .0001;
    const double thrust = burning && std::isfinite(native_thrust_n)
        ? std::max(0.0, native_thrust_n) * development_engine_gain(related_rpm,mach,augmented,settings) : 0.0;
    const double axis_z_m = engine == 0 ? -half_spacing_m : half_spacing_m;
    output.force_n.x += thrust;
    output.moment_n_m.y += (axis_z_m - mass.center_of_mass_m.z) * thrust;
    output.moment_n_m.z += (mass.center_of_mass_m.y - axis_y_m) * thrust;
    output.engines[engine].thrust_n = thrust;
    output.engines[engine].fuel_flow_kg_s = burning ? native_flow_kg_s : 0.0;
    output.engines[engine].combustion = burning;
    output.fuel_flow_kg_s += output.engines[engine].fuel_flow_kg_s;
    return thrust;
}
}
