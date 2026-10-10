// SPDX-License-Identifier: GPL-3.0-or-later
#include "f23b/performance/performance_model.hpp"
#include "f23b/performance/grinnelli_v21_reference.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace perf = f23b::performance;
namespace ref = f23b::performance::grinnelli_v21;

namespace {

struct Point {
    double mach{};
    double alpha_deg{};
    double net_force_n{};
    double lift_n{};
    double drag_coefficient{};
    double thrust_n{};
    double specific_excess_power_m_s{};
};

perf::Output evaluate(double mach, double alpha_deg, double density,
    double altitude_m, double throttle, double mass_kg = 16651.0,
    double pitch = 0.0, double roll = 0.0, double yaw = 0.0,
    double roll_rate = 0.0, double pitch_rate = 0.0, double yaw_rate = 0.0,
    double normal_g = 1.0) {
    perf::Model model;
    perf::Atmosphere atmosphere{density, 340.294, altitude_m};
    perf::MassState mass{};
    mass.mass_kg = mass_kg;
    perf::BodyState body{};
    body.velocity_body_m_s.x = mach * atmosphere.speed_of_sound_m_s;
    body.angle_of_attack_rad = alpha_deg * ref::deg_to_rad;
    body.angular_rate_rad_s = {roll_rate, yaw_rate, pitch_rate};
    body.altitude_agl_m = std::max(1000.0, altitude_m);
    body.normal_load_g = normal_g;
    perf::PilotInput input{};
    input.throttle_left = input.throttle_right = throttle;
    input.pitch = pitch;
    input.roll = roll;
    input.yaw = yaw;
    model.prime_engines_for_offline_analysis(throttle);
    return model.step(0.01, atmosphere, mass, body, input);
}

double level_alpha(double mach, double density, double altitude_m,
    double throttle, double mass_kg) {
    const double weight = mass_kg * 9.80665;
    double low = -5.0;
    double high = 65.0;
    for (int i = 0; i < 70; ++i) {
        const double mid = 0.5 * (low + high);
        const auto out = evaluate(mach, mid, density, altitude_m, throttle, mass_kg);
        if (out.force_n.y < weight) { low = mid; } else { high = mid; }
    }
    return 0.5 * (low + high);
}

std::vector<Point> sweep(double density, double altitude_m,
    double throttle, double mass_kg) {
    std::vector<Point> result;
    for (int i = 2; i <= 235; ++i) {
        const double mach = i / 100.0;
        const double alpha = level_alpha(mach, density, altitude_m, throttle, mass_kg);
        const auto out = evaluate(mach, alpha, density, altitude_m, throttle, mass_kg);
        const double speed = mach * 340.294;
        result.push_back({mach, alpha, out.force_n.x, out.force_n.y,
            out.drag_coefficient, out.engines[0].thrust_n + out.engines[1].thrust_n,
            out.force_n.x * speed / (mass_kg * 9.80665)});
    }
    return result;
}

double equilibrium_mach(const std::vector<Point>& points) {
    double last_positive = std::numeric_limits<double>::quiet_NaN();
    for (const auto& point : points) {
        if (point.net_force_n >= 0.0) {
            last_positive = point.mach;
        } else if (std::isfinite(last_positive)) {
            return last_positive;
        }
    }
    return last_positive;
}

std::optional<double> acceleration_time_s(const std::vector<Point>& points,
    double start_mach, double end_mach) {
    if (!(end_mach > start_mach)) { return std::nullopt; }
    double seconds = 0.0;
    bool saw = false;
    for (std::size_t i = 1; i < points.size(); ++i) {
        const auto& a = points[i - 1];
        const auto& b = points[i];
        if (b.mach <= start_mach || a.mach >= end_mach) { continue; }
        const double lo = std::max(a.mach, start_mach);
        const double hi = std::min(b.mach, end_mach);
        if (!(hi > lo)) { continue; }
        const double f0 = a.net_force_n;
        const double f1 = b.net_force_n;
        if (!(f0 > 0.0) || !(f1 > 0.0)) { return std::nullopt; }
        const double average_acceleration = 0.5 * (f0 + f1) / 16651.0;
        seconds += (hi - lo) * 340.294 / average_acceleration;
        saw = true;
    }
    return saw ? std::optional<double>{seconds} : std::nullopt;
}

struct TurnPoint {
    double mach{};
    double altitude_m{};
    double maximum_instantaneous_rate_deg_s{};
    double maximum_sustained_rate_deg_s{};
    double instantaneous_alpha_deg{};
    double sustained_alpha_deg{};
    double instantaneous_g{};
    double sustained_g{};
    double sustained_specific_excess_power_m_s{};
};

TurnPoint turn_point(double mach, double density, double altitude_m,
    double mass_kg) {
    TurnPoint result{};
    result.mach = mach;
    result.altitude_m = altitude_m;
    const double speed = mach * 340.294;
    const double weight = mass_kg * 9.80665;
    const double alpha_limit = perf::interpolate(ref::mach.data(),
        ref::allowed_alpha_deg.data(), ref::mach.size(), mach);
    double best_sustained_ps = -std::numeric_limits<double>::infinity();
    for (double alpha = 0.0; alpha <= alpha_limit; alpha += 0.25) {
        const auto out = evaluate(mach, alpha, density, altitude_m, 1.0, mass_kg);
        // The static aero table can produce far more lift than the FCS permits
        // at high dynamic pressure. Cap the performance estimate at the
        // reference positive-G command limit; this is a flight-control target,
        // not a structural-strength claim.
        const double aerodynamic_n = std::max(0.0, out.force_n.y / weight);
        const double n = std::min(aerodynamic_n, ref::positive_g_limit);
        const double turn_rate = n > 1.0 && speed > 1.0
            ? 9.80665 * std::sqrt(n * n - 1.0) / speed * ref::rad_to_deg : 0.0;
        if (turn_rate > result.maximum_instantaneous_rate_deg_s) {
            result.maximum_instantaneous_rate_deg_s = turn_rate;
            result.instantaneous_alpha_deg = alpha;
            result.instantaneous_g = n;
        }
        const double ps = out.force_n.x * speed / weight;
        if (out.force_n.x >= 0.0 && turn_rate > result.maximum_sustained_rate_deg_s) {
            result.maximum_sustained_rate_deg_s = turn_rate;
            result.sustained_alpha_deg = alpha;
            result.sustained_g = n;
            result.sustained_specific_excess_power_m_s = ps;
            best_sustained_ps = ps;
        }
    }
    if (!std::isfinite(best_sustained_ps)) {
        result.sustained_specific_excess_power_m_s =
            -std::numeric_limits<double>::infinity();
    }
    return result;
}

void write_csv(const std::filesystem::path& path,
    const std::vector<Point>& points) {
    std::ofstream stream(path);
    stream << "mach,alpha_deg,net_axial_force_n,lift_n,drag_coefficient,total_thrust_n,specific_excess_power_m_s\n";
    stream << std::setprecision(12);
    for (const auto& p : points) {
        stream << p.mach << ',' << p.alpha_deg << ',' << p.net_force_n << ','
               << p.lift_n << ',' << p.drag_coefficient << ',' << p.thrust_n << ','
               << p.specific_excess_power_m_s << '\n';
    }
}

void write_optional(std::ostream& stream, const std::optional<double>& value) {
    if (value && std::isfinite(*value)) { stream << *value; } else { stream << "null"; }
}

} // namespace

int main(int argc, char** argv) {
    const std::filesystem::path output = argc > 1
        ? std::filesystem::path(argv[1]) : std::filesystem::path("performance-report");
    std::filesystem::create_directories(output);

    struct Condition { const char* id; double density; double altitude_m; };
    const Condition conditions[] = {
        {"sea_level", 1.2250, 0.0},
        {"15000ft", 0.7708, 4572.0},
        {"30000ft", 0.4583, 9144.0},
        {"40000ft", 0.3027, 12192.0},
    };

    constexpr double mass_kg = 16651.0;
    std::ofstream summary(output / "summary.json");
    summary << std::setprecision(12);
    summary << "{\n  \"model\": \"f23b.grinnelli-v2.1-behavior-target.v1\",\n";
    summary << "  \"reference\": {\"tag\": \"v2.1.0\", \"commit\": "
            << "\"24dc1f51a8d0d9427c7bd3c368ccabd3e0ade53c\"},\n";
    summary << "  \"virtual_thrust_vectoring\": false,\n";
    summary << "  \"mass_kg\": " << mass_kg << ",\n";
    summary << "  \"conditions\": [\n";
    for (std::size_t i = 0; i < std::size(conditions); ++i) {
        const auto& c = conditions[i];
        const auto mil = sweep(c.density, c.altitude_m, 0.8256, mass_kg);
        const auto ab = sweep(c.density, c.altitude_m, 1.0, mass_kg);
        write_csv(output / (std::string(c.id) + "_mil.csv"), mil);
        write_csv(output / (std::string(c.id) + "_ab.csv"), ab);
        summary << "    {\"id\": \"" << c.id << "\", \"density_kg_m3\": "
                << c.density << ", \"altitude_m\": " << c.altitude_m
                << ", \"mil_equilibrium_mach\": " << equilibrium_mach(mil)
                << ", \"ab_equilibrium_mach\": " << equilibrium_mach(ab)
                << ", \"ab_acceleration_s\": {\"m0_5_to_0_9\": ";
        write_optional(summary, acceleration_time_s(ab, 0.5, 0.9));
        summary << ", \"m0_9_to_1_2\": ";
        write_optional(summary, acceleration_time_s(ab, 0.9, 1.2));
        summary << ", \"m1_2_to_1_6\": ";
        write_optional(summary, acceleration_time_s(ab, 1.2, 1.6));
        summary << ", \"m1_6_to_2_0\": ";
        write_optional(summary, acceleration_time_s(ab, 1.6, 2.0));
        summary << "}}";
        if (i + 1 != std::size(conditions)) { summary << ','; }
        summary << '\n';
    }
    summary << "  ]\n}\n";

    std::ofstream controls(output / "control_schedules.csv");
    controls << "mach,allowed_alpha_deg,max_roll_rate_rad_s,max_roll_rate_deg_s,max_elevator_deg,kd_pitch,kd_roll,kd_yaw\n";
    controls << std::setprecision(12);
    for (double mach = 0.0; mach <= 2.4 + 1.0e-9; mach += 0.05) {
        const double alpha = perf::interpolate(ref::mach.data(), ref::allowed_alpha_deg.data(), ref::mach.size(), mach);
        const double roll = perf::interpolate(ref::mach.data(), ref::maximum_roll_rate_rad_s.data(), ref::mach.size(), mach);
        const double elevator = perf::interpolate(ref::mach.data(), ref::maximum_elevator_deflection_deg.data(), ref::mach.size(), mach);
        const double kp = perf::interpolate(ref::mach.data(), ref::pitch_rate_damping.data(), ref::mach.size(), mach);
        const double kr = perf::interpolate(ref::mach.data(), ref::roll_rate_damping.data(), ref::mach.size(), mach);
        const double ky = perf::interpolate(ref::mach.data(), ref::yaw_rate_damping.data(), ref::mach.size(), mach);
        controls << mach << ',' << alpha << ',' << roll << ',' << roll * ref::rad_to_deg
                 << ',' << elevator << ',' << kp << ',' << kr << ',' << ky << '\n';
    }

    std::ofstream turns(output / "turn_performance.csv");
    turns << "mach,altitude_m,instantaneous_rate_deg_s,sustained_rate_deg_s,instantaneous_alpha_deg,sustained_alpha_deg,instantaneous_g,sustained_g,sustained_specific_excess_power_m_s\n";
    turns << std::setprecision(12);
    for (const auto& c : conditions) {
        if (c.altitude_m == 0.0 || c.altitude_m == 12192.0) { continue; }
        for (double mach : {0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.2}) {
            const auto p = turn_point(mach, c.density, c.altitude_m, mass_kg);
            turns << p.mach << ',' << p.altitude_m << ','
                  << p.maximum_instantaneous_rate_deg_s << ','
                  << p.maximum_sustained_rate_deg_s << ','
                  << p.instantaneous_alpha_deg << ',' << p.sustained_alpha_deg << ','
                  << p.instantaneous_g << ',' << p.sustained_g << ','
                  << p.sustained_specific_excess_power_m_s << '\n';
        }
    }

    std::ofstream response(output / "control_response.csv");
    response << "time_s,phase,pitch_command,roll_command,yaw_command,left_elevon_rad,right_elevon_rad,roll_moment_nm,pitch_moment_nm,yaw_moment_nm\n";
    response << std::setprecision(12);
    perf::Model response_model;
    perf::Atmosphere atmosphere{0.7708, 340.294, 4572.0};
    perf::MassState mass{};
    perf::BodyState body{};
    body.velocity_body_m_s.x = 0.8 * atmosphere.speed_of_sound_m_s;
    body.angle_of_attack_rad = 5.0 * ref::deg_to_rad;
    body.altitude_agl_m = 4572.0;
    body.normal_load_g = 1.0;
    perf::PilotInput input{};
    input.throttle_left = input.throttle_right = 0.75;
    constexpr double dt = 0.01;
    for (int step = 0; step < 500; ++step) {
        const double time = step * dt;
        const char* phase = time < 1.0 ? "neutral" : time < 2.0 ? "step" : "release";
        input.pitch = time >= 1.0 && time < 2.0 ? 0.65 : 0.0;
        input.roll = time >= 1.0 && time < 2.0 ? 0.65 : 0.0;
        const auto out = response_model.step(dt, atmosphere, mass, body, input);
        // This file measures command shaping and surface slew only. Body rates
        // remain fixed so the result is not confused with a home-grown rigid-
        // body integrator pretending to reproduce DCS.
        response << time << ',' << phase << ',' << out.controls.pitch_command << ','
                 << out.controls.roll_command << ',' << out.controls.yaw_command << ','
                 << out.controls.left_elevon_rad << ',' << out.controls.right_elevon_rad << ','
                 << out.moment_n_m.x << ',' << out.moment_n_m.z << ','
                 << out.moment_n_m.y << '\n';
    }

    std::cout << "Wrote offline performance report to " << output.string() << '\n';
    return 0;
}
