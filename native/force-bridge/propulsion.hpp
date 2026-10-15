// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include "../flight-core/bridge_model.hpp"
#include "../flight-core/development_options.hpp"

namespace f23b::bridge {
// Owner-requested moderate speed tune; project simulation settings.
inline constexpr double native_thrust_scale = 2.0; // previously 1.75 (+14.29%)
// PROJECT CHOICE (owner request, October 15): the native Hornet tanks hold 4,900 kg (10,800 lb).
// Burn scales as if the larger F-23B airframe held 18,000 lb: 10,800 / 18,000 = 0.6.
inline constexpr double fuel_economy = 0.6;
// Owner rule: burn follows applied thrust, times fuel_economy. Never below the native burn.
inline double extra_fuel_flow(double native_flow,double native_thrust,double applied_thrust,
    bool unlimited,const DryThrustSettings& settings) {
    if(unlimited||!settings.enabled||!std::isfinite(native_flow)||!std::isfinite(native_thrust)
       ||!std::isfinite(applied_thrust)||native_flow<=0||native_thrust<=0||applied_thrust<=native_thrust)return 0.;
    const double extra=native_flow*(applied_thrust/native_thrust*fuel_economy-1.);
    return std::isfinite(extra)?std::max(0.,extra):0.;
}
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
// Recorded run-env-15k, Mach 0.9 to 1.0: mean applied idle thrust / mean
// applied maximum-dry thrust. Native spool and fuel flow remain unchanged.
inline constexpr double supersonic_idle_fraction = 0.07753250092245408;
inline double supersonic_idle_factor(double mach,double throttle,const DryThrustSettings& settings) {
    // candidate-2 disables the development dry settings, preserving its thrust.
    if(!settings.enabled||!std::isfinite(mach)||!std::isfinite(throttle)||mach<=1.2||throttle>=.775)return 1.;
    const double blend=std::clamp((mach-1.2)/.1,0.,1.);
    const double weight=blend*blend*(3.-2.*blend);
    return 1.-weight*(1.-supersonic_idle_fraction)*(1.-std::clamp(throttle/.775,0.,1.));
}
inline double add_native_engine(performance::Output& output,
    const performance::MassState& mass, std::size_t engine,
    double native_thrust_n, double native_flow_kg_s, double related_rpm,
    double mach=0.,bool augmented=false,const DryThrustSettings& settings={},double throttle=.775) {
    // Accepted engine-axis locations in VisualConfig.lua. Straight axial thrust;
    // no virtual vectoring or independently running second engine simulation.
    constexpr double axis_y_m = -0.320670754;
    constexpr double half_spacing_m = 1.311020017;
    const bool burning = std::isfinite(native_flow_kg_s) && native_flow_kg_s > .0001;
    const double thrust = burning && std::isfinite(native_thrust_n)
        ? std::max(0.0, native_thrust_n) * development_engine_gain(related_rpm,mach,augmented,settings)
            * supersonic_idle_factor(mach,throttle,settings) : 0.0;
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
