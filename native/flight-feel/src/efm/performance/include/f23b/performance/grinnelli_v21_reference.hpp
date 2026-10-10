// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * F-23B Grinnelli-v2.1 behavioral reference data.
 *
 * Adapted 2026-09-03 from:
 *   Grinnelli Designs F-22A, tag v2.1.0
 *   commit 24dc1f51a8d0d9427c7bd3c368ccabd3e0ade53c
 *   EFM/source/RAPTOR.h and EFM/source/RAPTOR.cpp
 *
 * Upstream notices identify Copyright (C) 2025 Branden Hooper and license the
 * relevant source under GPL-3.0-or-later. This file is modified for the F-23B
 * performance branch. It contains no Grinnelli binary, model, texture, sound,
 * mission, or manual content.
 */
#pragma once

#include <array>
#include <cstddef>

namespace f23b::performance::grinnelli_v21 {

inline constexpr double pi = 3.1415926535897932384626433832795;
inline constexpr double deg_to_rad = pi / 180.0;
inline constexpr double rad_to_deg = 180.0 / pi;

inline constexpr std::array<double, 13> mach{
    0.0, 0.2, 0.4, 0.6, 0.8, 1.0, 1.2, 1.4, 1.6, 1.8, 2.0, 2.2, 2.4};
inline constexpr std::array<double, 13> zero_lift_drag{
    0.0130, 0.0135, 0.0140, 0.0210, 0.0358, 0.0505, 0.0455,
    0.04175, 0.0380, 0.0370, 0.0368, 0.0362, 0.0390};
inline constexpr std::array<double, 13> lift_per_degree{
    0.0150, 0.0550, 0.0700, 0.0650, 0.0600, 0.0550, 0.0500,
    0.0450, 0.0400, 0.0375, 0.0350, 0.0325, 0.0300};
inline constexpr std::array<double, 13> maximum_roll_rate_rad_s{
    1.65, 2.45, 3.25, 4.70, 3.98, 3.20, 2.50,
    2.25, 2.00, 1.85, 1.70, 1.50, 1.30};
inline constexpr std::array<double, 13> allowed_alpha_deg{
    60.0, 57.5, 55.0, 50.0, 45.0, 40.0, 35.0,
    32.5, 30.0, 30.0, 30.0, 30.0, 30.0};
inline constexpr std::array<double, 13> maximum_lift_coefficient{
    1.80, 1.85, 1.90, 1.85, 1.75, 1.60, 1.45,
    1.35, 1.25, 1.15, 1.10, 1.05, 1.00};

inline constexpr std::array<double, 10> alpha_drag_alpha_deg{
    0.0, 10.0, 20.0, 30.0, 40.0, 50.0, 60.0, 70.0, 80.0, 90.0};
inline constexpr std::array<double, 10> alpha_drag_coefficient{
    0.0, 0.005, 0.020, 0.082, 0.170, 0.275, 0.410, 0.580, 0.700, 0.810};
inline constexpr std::array<double, 5> beta_deg{0.0, 5.0, 10.0, 20.0, 30.0};
inline constexpr std::array<double, 5> beta_force_coefficient{0.0, 0.05, 0.10, 0.20, 0.30};

inline constexpr std::array<double, 13> idle_thrust_per_engine_n{
    6000.0, 8500.0, 10500.0, 11500.0, 12000.0, 12500.0, 13000.0,
    13000.0, 13000.0, 13000.0, 13000.0, 13000.0, 13000.0};
inline constexpr std::array<double, 13> maximum_dry_thrust_per_engine_n{
    115600.0, 117050.0, 118500.0, 127250.0, 154000.0, 183800.0, 245000.0,
    293000.0, 330000.0, 230000.0, 0.0, 0.0, 0.0};
inline constexpr std::array<double, 13> maximum_ab_thrust_per_engine_n{
    197000.0, 215300.0, 232600.0, 256500.0, 275000.0, 298000.0, 325000.0,
    355000.0, 405000.0, 470000.0, 535000.0, 536500.0, 0.0};

inline constexpr std::array<double, 13> elevator_rate_rad_s{
    1.396, 1.396, 1.300, 1.016, 0.912, 0.912, 0.912,
    0.942, 1.105, 1.105, 1.105, 1.105, 1.105};
inline constexpr std::array<double, 13> maximum_elevator_deflection_deg{
    30.0, 28.0, 21.5, 13.8, 11.5, 11.0, 11.0,
    11.0, 13.0, 13.0, 13.0, 13.0, 13.0};
inline constexpr double maximum_aileron_command_rate_per_s = 14.55;

inline constexpr std::array<double, 13> pitch_rate_damping{
    0.675, 0.668, 0.654, 0.800, 1.250, 1.950, 2.200,
    2.500, 2.800, 2.800, 2.800, 2.800, 2.800};
inline constexpr std::array<double, 13> roll_rate_damping{
    0.500, 0.350, 0.330, 0.290, 0.330, 0.370, 0.420,
    0.520, 0.620, 0.740, 0.850, 0.880, 0.810};
inline constexpr std::array<double, 13> yaw_rate_damping{
    2.0, 2.0, 2.0, 2.1, 2.2, 2.3, 2.4,
    2.5, 2.6, 2.7, 2.8, 2.9, 3.0};

inline constexpr double wing_area_m2 = 78.06;
inline constexpr double wing_span_m = 13.56;
inline constexpr double length_m = 18.92;
inline constexpr double height_m = 5.08;
inline constexpr double zero_lift_bias = 0.008;
inline constexpr double beta_bias = -0.028;
inline constexpr double gear_drag = 0.14;
inline constexpr double speedbrake_drag = 0.062;
inline constexpr double flap_drag = 0.042;
inline constexpr double flap_lift = 0.048;

inline constexpr double reference_empty_mass_kg = 19700.0;
inline constexpr double reference_internal_fuel_kg = 8200.0;
inline constexpr double reference_nominal_mass_kg =
    reference_empty_mass_kg + reference_internal_fuel_kg;

// The upstream EFM ignores the inertia values delivered by DCS. The following
// normalization tensor is therefore explicitly an F-23B integration value,
// not an upstream claim. It is kept injectable so owner flight evidence can
// refine angular-response normalization without changing the reference tables.
inline constexpr std::array<double, 3> provisional_reference_inertia_kg_m2{
    45000.0, 920000.0, 950000.0}; // roll, yaw, pitch

inline constexpr double positive_g_limit = 11.0;
inline constexpr double negative_g_limit = -4.0;
inline constexpr double input_slew_rate_per_s = 1.33;
inline constexpr double command_slew_rate_per_s = 40.0;
inline constexpr double autotrim_gain = 0.06;
inline constexpr double autotrim_max_command = 0.28;
inline constexpr double autotrim_rate_per_s = 0.06;
inline constexpr double autotrim_fade_rate_per_s = 0.10;
inline constexpr double beta_integral_gain = 0.06;
inline constexpr double beta_integral_limit = 0.01;
inline constexpr double beta_integral_decay_per_s = 0.07;
inline constexpr double static_maximum_dry_thrust_per_engine_n = 112654.0;
inline constexpr double static_maximum_ab_thrust_per_engine_n = 155688.0;
inline constexpr double throttle_idle = 0.67;
inline constexpr double throttle_mil = 1.025;
inline constexpr double throttle_max = 1.10;
inline constexpr double afterburner_light_threshold = 1.035;
inline constexpr double afterburner_spool_time_air_s = 1.10;
inline constexpr double afterburner_spool_time_ground_s = 1.65;
inline constexpr double spool_up_rate_air_per_s = 0.1285;
inline constexpr double spool_down_rate_air_per_s = 0.1220;
inline constexpr double spool_up_rate_ground_per_s = 0.0790;
inline constexpr double spool_down_rate_ground_per_s = 0.0813;
inline constexpr double idle_rpm_fraction = 0.675;
inline constexpr double maximum_rpm = 150.0;
inline constexpr double idle_rpm = 50.0;

} // namespace f23b::performance::grinnelli_v21
