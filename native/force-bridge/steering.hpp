// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>

namespace f23b::bridge {
// Wheel centres: F-23 nose 5.651507 m and mains -1.401347 m (accepted BLEND);
// Hornet nose 3.02 m and mains -2.319 m (installed FA-18C.lua). The native NWS
// law commands Hornet angles, so the same pedal turned the F-23B 32 percent wider.
inline constexpr double wheelbase_m = 5.651507 + 1.401347;
inline constexpr double wheelbase_ratio = wheelbase_m / (3.02 + 2.319);
// Native Hornet nosewheel yaw_limit (installed FM/config.lua).
inline constexpr double nose_yaw_limit_rad = 75.0 * 3.14159265358979323846 / 180.0;
// Largest lateral acceleration the nosewheel may command. At 26 m/s, half pedal with full
// brakes commanded about 1.4 g. The jet ground-looped, rolled 25 degrees and damaged its gear,
// and NWS stopped working. Up to 7 m/s the full low-gain angle stays within this limit.
inline constexpr double nose_lateral_limit_m_s2 = 0.3 * 9.80665;

// Returns the nosewheel angle that gives the Hornet turn radius on the F-23 wheelbase.
inline double nose_wheel_yaw(double native_rad) {
    if (!std::isfinite(native_rad) || native_rad == 0.0) return native_rad;
    const double magnitude = std::min(std::abs(native_rad), nose_yaw_limit_rad);
    const double scaled = std::atan(std::tan(magnitude) * wheelbase_ratio);
    return std::copysign(std::min(scaled, nose_yaw_limit_rad), native_rad);
}

// Same angle, limited so the turn at this ground speed stays within nose_lateral_limit_m_s2.
inline double nose_wheel_yaw(double native_rad, double ground_speed_m_s) {
    const double yaw = nose_wheel_yaw(native_rad);
    if (!std::isfinite(yaw) || !std::isfinite(ground_speed_m_s) || ground_speed_m_s == 0.0) return yaw;
    const double limit = std::atan(wheelbase_m * nose_lateral_limit_m_s2 / (ground_speed_m_s * ground_speed_m_s));
    return std::clamp(yaw, -limit, limit);
}
}
