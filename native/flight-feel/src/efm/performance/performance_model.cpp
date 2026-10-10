// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * F-23B performance model adapted from the behavior of Grinnelli F-22A v2.1.0.
 * Copyright (C) 2025 Branden Hooper (upstream portions/behavior)
 * Copyright (C) 2026 F-23B project contributors (adaptation)
 */
#include "f23b/performance/performance_model.hpp"
#include "f23b/performance/grinnelli_v21_reference.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace f23b::performance {
namespace ref = grinnelli_v21;
namespace {

constexpr double kEpsilon = 1.0e-9;

[[nodiscard]] bool finite(double value) noexcept {
    return std::isfinite(value);
}

[[nodiscard]] double safe_value(double value, double fallback = 0.0) noexcept {
    return finite(value) ? value : fallback;
}

[[nodiscard]] Vec3 cross(const Vec3& a, const Vec3& b) noexcept {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

void add(Vec3& target, const Vec3& value) noexcept {
    target.x += value.x;
    target.y += value.y;
    target.z += value.z;
}

void add_force(Output& output, const MassState& mass,
    const Vec3& force, const Vec3& application_point) noexcept {
    add(output.force_n, force);
    const Vec3 arm{
        application_point.x - mass.center_of_mass_m.x,
        application_point.y - mass.center_of_mass_m.y,
        application_point.z - mass.center_of_mass_m.z,
    };
    add(output.moment_n_m, cross(arm, force));
}

[[nodiscard]] double sample(const std::array<double, 13>& values, double mach) noexcept {
    return interpolate(ref::mach.data(), values.data(), values.size(), mach);
}

[[nodiscard]] double sample_alpha_drag(double alpha_deg) noexcept {
    return interpolate(ref::alpha_drag_alpha_deg.data(), ref::alpha_drag_coefficient.data(),
        ref::alpha_drag_alpha_deg.size(), std::abs(alpha_deg));
}

[[nodiscard]] double sample_beta_force(double beta_deg) noexcept {
    return interpolate(ref::beta_deg.data(), ref::beta_force_coefficient.data(),
        ref::beta_deg.size(), std::abs(beta_deg));
}

[[nodiscard]] double density_ratio(double density_kg_m3) noexcept {
    return clamp(safe_value(density_kg_m3, 1.225) / 1.225, 0.0, 1.25);
}

[[nodiscard]] double rate_limit(double previous, double target,
    double rate_per_second, double dt) noexcept {
    return approach(previous, target, std::max(0.0, rate_per_second * dt));
}

[[nodiscard]] double directional_alpha_margin(double alpha_deg,
    double allowed_alpha_deg, double pitch_command) noexcept {
    constexpr double blend_width_deg = 8.0;
    if (pitch_command > 0.0) {
        return clamp((allowed_alpha_deg - alpha_deg) / blend_width_deg, 0.0, 1.0);
    }
    if (pitch_command < 0.0) {
        return clamp((allowed_alpha_deg + alpha_deg) / blend_width_deg, 0.0, 1.0);
    }
    return 1.0;
}

[[nodiscard]] double spool_to_visual(double throttle_state) noexcept {
    return clamp((throttle_state - ref::throttle_idle)
        / (ref::throttle_max - ref::throttle_idle), 0.0, 1.0);
}

} // namespace

double clamp(double value, double low, double high) noexcept {
    if (!finite(value)) { return low; }
    return std::max(low, std::min(value, high));
}

double interpolate(const double* xs, const double* ys,
    std::size_t count, double x) noexcept {
    if (xs == nullptr || ys == nullptr || count == 0 || !finite(x)) { return 0.0; }
    if (x <= xs[0]) { return ys[0]; }
    if (x >= xs[count - 1]) { return ys[count - 1]; }
    for (std::size_t i = 1; i < count; ++i) {
        if (x <= xs[i]) {
            const double width = xs[i] - xs[i - 1];
            if (std::abs(width) <= kEpsilon) { return ys[i]; }
            const double t = (x - xs[i - 1]) / width;
            return ys[i - 1] + t * (ys[i] - ys[i - 1]);
        }
    }
    return ys[count - 1];
}

double shaped_axis(double input) noexcept {
    const double bounded = clamp(input, -1.0, 1.0);
    return bounded * std::abs(bounded);
}

double approach(double current, double target, double maximum_delta) noexcept {
    if (!finite(current) || !finite(target) || !finite(maximum_delta)) { return target; }
    const double delta = target - current;
    return current + clamp(delta, -std::abs(maximum_delta), std::abs(maximum_delta));
}

Model::Model(Options options) noexcept : options_(options) {
    reset();
}

void Model::reset() noexcept {
    output_ = {};
    for (auto& engine : engines_) {
        engine.throttle_state = ref::throttle_idle;
        engine.afterburner_fraction = 0.0;
    }
    smoothed_pitch_discrete_ = 0.0;
    smoothed_roll_discrete_ = 0.0;
    smoothed_yaw_discrete_ = 0.0;
    autotrim_command_ = 0.0;
    takeoff_trim_command_ = 0.0;
    beta_integral_ = 0.0;
    last_yaw_input_ = 0.0;
    last_pitch_command_ = 0.0;
    last_roll_command_ = 0.0;
    last_yaw_command_ = 0.0;
    left_elevon_rad_ = 0.0;
    right_elevon_rad_ = 0.0;
    flap_position_ = 0.0;
    leading_edge_flap_position_ = 0.0;
    simulation_time_s_ = 0.0;
    airborne_yaw_time_s_ = 0.5; // preserve an airborne start; ground contact resets it
}

void Model::prime_engines_for_offline_analysis(double throttle,
    bool left_running, bool right_running) noexcept {
    const double command = clamp(throttle, 0.0, 1.0);
    const std::array<bool, 2> running{left_running, right_running};
    for (std::size_t i = 0; i < engines_.size(); ++i) {
        if (!running[i]) {
            engines_[i].throttle_state = 0.0;
            engines_[i].afterburner_fraction = 0.0;
            continue;
        }
        engines_[i].throttle_state = ref::throttle_idle
            + command * (ref::throttle_max - ref::throttle_idle);
        engines_[i].afterburner_fraction =
            engines_[i].throttle_state > ref::afterburner_light_threshold ? 1.0 : 0.0;
    }
}

Output Model::step(double dt_s, const Atmosphere& atmosphere,
    const MassState& mass, const BodyState& body, const PilotInput& input) noexcept {
    const double dt = clamp(dt_s, 0.0, 0.05);
    simulation_time_s_ += dt;
    output_ = {};

    const Vec3 airflow{
        safe_value(body.velocity_body_m_s.x) - safe_value(body.wind_body_m_s.x),
        safe_value(body.velocity_body_m_s.y) - safe_value(body.wind_body_m_s.y),
        safe_value(body.velocity_body_m_s.z) - safe_value(body.wind_body_m_s.z),
    };
    const double speed_squared = airflow.x * airflow.x + airflow.y * airflow.y
        + airflow.z * airflow.z;
    const double speed = std::sqrt(std::max(0.0, speed_squared));
    const double speed_of_sound = std::max(1.0, safe_value(atmosphere.speed_of_sound_m_s, 340.294));
    const double mach = speed / speed_of_sound;
    const double rho = std::max(0.0, safe_value(atmosphere.density_kg_m3, 1.225));
    const double q = 0.5 * rho * speed_squared;
    const double ias = speed * std::sqrt(rho / 1.225);
    const double ias_knots = ias * 1.94384449244;
    const double alpha_deg = safe_value(body.angle_of_attack_rad) * ref::rad_to_deg;
    const double beta_deg = safe_value(body.sideslip_rad) * ref::rad_to_deg;
    const bool on_ground = body.weight_on_wheels;

    output_.airspeed_m_s = speed;
    output_.indicated_airspeed_m_s = ias;
    output_.mach = mach;
    output_.dynamic_pressure_pa = q;

    // Discrete inputs inherit the upstream slew behavior. Analog controls stay
    // direct so a real stick is not delayed by a keyboard-oriented filter.
    smoothed_pitch_discrete_ = rate_limit(smoothed_pitch_discrete_, input.pitch,
        ref::input_slew_rate_per_s, dt);
    smoothed_roll_discrete_ = rate_limit(smoothed_roll_discrete_, input.roll,
        ref::input_slew_rate_per_s, dt);
    smoothed_yaw_discrete_ = rate_limit(smoothed_yaw_discrete_, input.yaw,
        ref::input_slew_rate_per_s, dt);
    const double pitch_source = input.analog_pitch
        ? clamp(input.pitch, -1.0, 1.0) : smoothed_pitch_discrete_;
    const double roll_source = input.analog_roll
        ? clamp(input.roll, -1.0, 1.0) : smoothed_roll_discrete_;
    const double yaw_source = input.analog_yaw
        ? clamp(input.yaw, -1.0, 1.0) : smoothed_yaw_discrete_;

    // Automatic configuration scheduling follows the reference intent while
    // preserving explicit gear/flap inputs supplied by the eventual DCS adapter.
    const double flap_target = input.landing_flaps
        || (input.gear_down && ias_knots < 250.0) ? 1.0 : 0.0;
    flap_position_ = approach(flap_position_, flap_target, 0.75 * dt);
    const double lef_target = mach < 0.8
        ? clamp((alpha_deg - 10.0) / 10.0, 0.0, 1.0) : 0.0;
    leading_edge_flap_position_ = approach(
        leading_edge_flap_position_, lef_target, 0.65 * dt);

    const double kd_pitch = sample(ref::pitch_rate_damping, mach);
    double kd_roll = sample(ref::roll_rate_damping, mach);
    const double kd_yaw = sample(ref::yaw_rate_damping, mach);
    const double allowed_alpha = sample(ref::allowed_alpha_deg, mach);
    const double roll_rate_limit = sample(ref::maximum_roll_rate_rad_s, mach);

    double pitch_sensitivity = 1.0;
    if (mach >= 0.67) {
        const double delta = mach - 0.67;
        pitch_sensitivity = clamp(1.0 - 0.229 * delta * delta, 0.30, 1.0);
    }

    const bool autotrim_disabled = on_ground || std::abs(body.roll_rad) > 1.2217
        || alpha_deg >= 9.0 || ias_knots < 160.0;
    if (!autotrim_disabled) {
        const double pitch_rate_deg_s = body.angular_rate_rad_s.z * ref::rad_to_deg;
        const double q_ref = 0.5 * 1.225 * 200.0 * 200.0;
        const double q_scale = q > 100.0 ? q_ref / q : 1.0;
        double pitch_rate_error = 0.0;
        constexpr double input_deadzone = 0.025;
        if (pitch_source < -input_deadzone) {
            if (pitch_rate_deg_s >= 0.0) { pitch_rate_error = -pitch_rate_deg_s; }
        } else if (pitch_source > input_deadzone) {
            if (pitch_rate_deg_s <= 0.0) { pitch_rate_error = -pitch_rate_deg_s; }
        } else {
            pitch_rate_error = -pitch_rate_deg_s;
        }
        // Gain is calibrated at 100 Hz; integrate it in seconds at every step.
        const double delta = clamp(pitch_rate_error * ref::autotrim_gain * q_scale * (dt / 0.01),
            -ref::autotrim_rate_per_s * dt, ref::autotrim_rate_per_s * dt);
        autotrim_command_ = clamp(autotrim_command_ + delta,
            -ref::autotrim_max_command, ref::autotrim_max_command);
    } else {
        autotrim_command_ = approach(autotrim_command_, 0.0,
            ref::autotrim_fade_rate_per_s * dt);
    }

    const bool takeoff_trim_enabled = on_ground && ias_knots < 170.0
        && std::abs(pitch_source) < 0.05;
    takeoff_trim_command_ = approach(takeoff_trim_command_,
        takeoff_trim_enabled ? 0.25 : 0.0, 0.05 * dt);

    // Bound feedback stiffness using the actual mass callback and both roll
    // effectors. The old near-center gain jump excited a two-step slew cycle.
    // Use full elevon authority as a conservative bound even at high alpha or
    // while its actuator is lagging. Scale the matching pilot term as well:
    // this preserves the scheduled steady roll-rate demand at high pressure.
    const double roll_authority = q * ref::wing_area_m2 * ref::wing_span_m
        * (0.25 * 30.0 * ref::deg_to_rad
            + 0.10 * sample(ref::maximum_elevator_deflection_deg, mach) * ref::deg_to_rad);
    const double roll_feedback_rate = kd_roll * roll_authority
        / std::max(1.0, mass.inertia_kg_m2.x);
    // Smoothly limit decay to 100/s and less than half a step. This leaves
    // margin for actuator lag instead of approaching the Euler limit of two.
    const double roll_response_scale = 1.0
        / (1.0 + roll_feedback_rate * std::max(0.01, 2.0 * dt));
    kd_roll *= roll_response_scale;

    double pitch_command = shaped_axis(pitch_source) * 1.1 * pitch_sensitivity;
    pitch_command += -body.angular_rate_rad_s.z * kd_pitch;
    pitch_command += autotrim_command_ + takeoff_trim_command_;
    pitch_command = clamp(pitch_command, -1.0, 1.0);

    const double alpha_margin = directional_alpha_margin(alpha_deg, allowed_alpha, pitch_command);
    if (alpha_margin < 1.0) {
        output_.controls.alpha_limiter_active = true;
        pitch_command *= alpha_margin;
    }
    if (body.normal_load_g > ref::positive_g_limit && pitch_command > 0.0) {
        pitch_command = std::max(0.0,
            pitch_command - (body.normal_load_g - ref::positive_g_limit) * 0.10);
        output_.controls.g_limiter_active = true;
    } else if (body.normal_load_g < ref::negative_g_limit && pitch_command < 0.0) {
        pitch_command = std::min(0.0,
            pitch_command + (ref::negative_g_limit - body.normal_load_g) * 0.10);
        output_.controls.g_limiter_active = true;
    }
    pitch_command = rate_limit(last_pitch_command_, pitch_command,
        ref::command_slew_rate_per_s, dt);
    last_pitch_command_ = pitch_command;

    double roll_command = shaped_axis(roll_source) * roll_response_scale
        - body.angular_rate_rad_s.x * kd_roll;
    // The sixth-power roll limiter is preserved as a smooth command-domain
    // barrier rather than a frame-dependent impulse.
    const double roll_ratio = std::abs(body.angular_rate_rad_s.x)
        / std::max(0.1, roll_rate_limit + 0.1);
    if (roll_ratio > 0.90 && roll_command * body.angular_rate_rad_s.x > 0.0) {
        const double limiter = clamp((roll_ratio - 0.90) / 0.20, 0.0, 1.0);
        roll_command *= 1.0 - limiter;
    }
    roll_command = rate_limit(last_roll_command_, clamp(roll_command, -1.0, 1.0),
        ref::maximum_aileron_command_rate_per_s, dt);
    last_roll_command_ = roll_command;

    const double q_coordination = q / (q + 10000.0);
    const double adverse_yaw_gain = 0.3 * (1.0 - std::min(mach, 1.0));
    const double adverse_yaw = clamp(roll_command * adverse_yaw_gain * q_coordination,
        -0.3, 0.3);
    const double beta_gain = mach > 0.9
        ? clamp(0.1 * (mach - 0.9) / 0.3, 0.0, 0.1) : 0.0;
    double yaw_command = 0.0;
    if (std::abs(yaw_source) < 0.01) {
        yaw_command = -body.angular_rate_rad_s.y * kd_yaw
            - clamp(beta_deg, -3.0, 3.0) * clamp(last_yaw_input_, 0.0, 0.3)
            - beta_gain * beta_deg + adverse_yaw;
        if (std::abs(beta_deg) < 3.0 && !on_ground && std::abs(last_yaw_command_) < 0.9) {
            beta_integral_ = clamp(beta_integral_ + beta_deg
                * ref::beta_integral_gain * dt,
                -ref::beta_integral_limit, ref::beta_integral_limit);
        } else {
            beta_integral_ *= std::exp(-ref::beta_integral_decay_per_s * dt);
        }
        yaw_command -= beta_integral_;
        if (alpha_deg > 10.0) {
            const double pitch_yaw_coupling = -body.angular_rate_rad_s.z * 0.11
                * alpha_deg / std::max(1.0, allowed_alpha);
            yaw_command += clamp(pitch_yaw_coupling, -0.5, 0.5);
        }
        last_yaw_input_ = std::max(0.0, last_yaw_input_ - dt / 0.5);
    } else {
        yaw_command = 0.5 * yaw_source * std::sqrt(std::abs(yaw_source))
            - body.angular_rate_rad_s.y * kd_yaw
            - beta_gain * beta_deg * 0.5 + adverse_yaw * 0.5;
        beta_integral_ *= std::exp(-ref::beta_integral_decay_per_s * dt);
        last_yaw_input_ = 0.5 * std::abs(yaw_source);
    }
    yaw_command = rate_limit(last_yaw_command_, clamp(yaw_command, -1.0, 1.0),
        ref::command_slew_rate_per_s, dt);
    last_yaw_command_ = yaw_command;

    const double left_elevon_command = clamp(pitch_command - roll_command, -1.0, 1.0);
    const double right_elevon_command = clamp(pitch_command + roll_command, -1.0, 1.0);
    const double max_elevator_rad = sample(ref::maximum_elevator_deflection_deg, mach)
        * ref::deg_to_rad;
    const double elevator_rate = sample(ref::elevator_rate_rad_s, mach);
    left_elevon_rad_ = approach(left_elevon_rad_,
        -left_elevon_command * max_elevator_rad, elevator_rate * dt);
    right_elevon_rad_ = approach(right_elevon_rad_,
        -right_elevon_command * max_elevator_rad, elevator_rate * dt);

    output_.controls.pitch_command = pitch_command;
    output_.controls.roll_command = roll_command;
    output_.controls.yaw_command = yaw_command;
    output_.controls.left_elevon_rad = left_elevon_rad_;
    output_.controls.right_elevon_rad = right_elevon_rad_;
    output_.controls.left_aileron = roll_command;
    output_.controls.right_aileron = -roll_command;
    output_.controls.left_rudder = yaw_command;
    output_.controls.right_rudder = yaw_command;
    output_.controls.flap = flap_position_;
    output_.controls.leading_edge_flap = leading_edge_flap_position_;
    output_.controls.angle_of_attack_limit_deg = allowed_alpha;
    output_.controls.roll_rate_limit_rad_s = roll_rate_limit;

    // Propulsion. The hot-air owner candidate starts both engines running; the
    // state still honors per-engine combustion flags so later integration can
    // bridge start/shutdown without changing the flight equations.
    const std::array<bool, 2> engine_running{
        input.engine_left_running, input.engine_right_running};
    for (std::size_t i = 0; i < engines_.size(); ++i) {
        auto& state = engines_[i];
        auto& engine = output_.engines[i];
        const bool running = engine_running[i];
        const double target = running
            ? ref::throttle_idle + clamp(i == 0 ? input.throttle_left : input.throttle_right, 0.0, 1.0)
                * (ref::throttle_max - ref::throttle_idle)
            : 0.0;
        const double up_rate = on_ground
            ? ref::spool_up_rate_ground_per_s : ref::spool_up_rate_air_per_s;
        const double down_rate = on_ground
            ? ref::spool_down_rate_ground_per_s : ref::spool_down_rate_air_per_s;
        state.throttle_state = approach(state.throttle_state, target,
            (target >= state.throttle_state ? up_rate : down_rate) * dt);
        if (running) {
            state.throttle_state = clamp(state.throttle_state,
                ref::throttle_idle, ref::throttle_max);
        } else {
            state.throttle_state = clamp(state.throttle_state, 0.0, ref::throttle_max);
        }

        const double ab_time = on_ground
            ? ref::afterburner_spool_time_ground_s : ref::afterburner_spool_time_air_s;
        const bool ab_commanded = running
            && state.throttle_state >= ref::afterburner_light_threshold
            && target > ref::afterburner_light_threshold;
        state.afterburner_fraction = clamp(state.afterburner_fraction
            + (ab_commanded ? dt / ab_time : -dt / ab_time), 0.0, 1.0);

        const double idle_thrust = sample(ref::idle_thrust_per_engine_n, mach);
        const double scheduled_dry = on_ground
            ? ref::static_maximum_dry_thrust_per_engine_n
            : sample(ref::maximum_dry_thrust_per_engine_n, mach);
        const double scheduled_ab = on_ground
            ? ref::static_maximum_ab_thrust_per_engine_n
            : sample(ref::maximum_ab_thrust_per_engine_n, mach);
        const double atmospheric_factor = density_ratio(rho);
        double thrust = 0.0;
        if (running) {
            if (state.throttle_state <= ref::throttle_mil) {
                const double normalized = clamp((state.throttle_state - ref::throttle_idle)
                    / (ref::throttle_mil - ref::throttle_idle), 0.0, 1.0);
                const double factor = normalized * normalized;
                thrust = (idle_thrust + (scheduled_dry - idle_thrust) * factor)
                    * atmospheric_factor;
            } else {
                const double normalized = clamp((state.throttle_state - ref::throttle_mil)
                    / (ref::throttle_max - ref::throttle_mil), 0.0, 1.0);
                const double adjusted = normalized <= 0.0667
                    ? 0.0 : (normalized - 0.0667) / (1.0 - 0.0667);
                thrust = (scheduled_dry + (scheduled_ab - scheduled_dry)
                    * adjusted * adjusted * state.afterburner_fraction)
                    * atmospheric_factor;
            }
        }

        const double mach_factor = 1.0 + 0.5 * mach;
        double fuel_flow = 0.0;
        if (running) {
            if (state.throttle_state <= ref::throttle_mil) {
                const double throttle_factor = clamp((state.throttle_state - ref::throttle_idle)
                    / (ref::throttle_mil - ref::throttle_idle), 0.0, 1.0);
                fuel_flow = (0.126 + (1.5 - 0.126) * throttle_factor)
                    * atmospheric_factor * mach_factor;
            } else {
                const double ab_factor = clamp((state.throttle_state - ref::throttle_mil)
                    / (ref::throttle_max - ref::throttle_mil), 0.0, 1.0);
                fuel_flow = (1.5 + (6.5 - 1.5) * ab_factor)
                    * atmospheric_factor * mach_factor;
            }
        }

        engine.throttle_state = state.throttle_state;
        engine.spool_fraction = running ? spool_to_visual(state.throttle_state) : 0.0;
        engine.afterburner_fraction = state.afterburner_fraction;
        engine.thrust_n = std::max(0.0, thrust);
        engine.fuel_flow_kg_s = std::max(0.0, fuel_flow);
        engine.rpm = running ? ref::idle_rpm
            + (ref::maximum_rpm - ref::idle_rpm) * engine.spool_fraction : 0.0;
        engine.combustion = running;
        output_.fuel_flow_kg_s += engine.fuel_flow_kg_s;
    }

    const double cy_alpha = sample(ref::lift_per_degree, mach);
    double cy_max = sample(ref::maximum_lift_coefficient, mach);
    if (std::abs(alpha_deg) >= 15.0 && std::abs(alpha_deg) <= 65.0) {
        cy_max += 0.72 * std::exp(-std::pow(std::abs(alpha_deg) - 35.0, 2.0) / 1200.0);
    }
    cy_max += ref::flap_lift * leading_edge_flap_position_;
    double cy = clamp(cy_alpha * alpha_deg, -cy_max, cy_max);
    if (std::abs(alpha_deg) >= 90.0) {
        const double lift_reduction = clamp(1.0
            - ((std::abs(alpha_deg) - 90.0) / 60.0) * 0.9, 0.1, 1.0);
        cy *= lift_reduction;
    }
    const double lift_coefficient = cy + ref::zero_lift_bias
        + ref::flap_lift * flap_position_;

    double cy_beta = sample_beta_force(beta_deg);
    if (alpha_deg > 30.0) { cy_beta *= 1.5; }
    const double tail_side_coefficient = (0.5 * cy_alpha + ref::beta_bias) * beta_deg
        + (beta_deg < 0.0 ? -cy_beta : cy_beta);

    const double cd0 = sample(ref::zero_lift_drag, mach);
    const double transonic_factor = 1.0
        + 0.85 * std::max(0.0, (mach - 0.9) / 0.3)
            * (1.0 - std::min(1.0, (mach - 1.2) / 1.0))
            * std::max(0.0, 1.0 - safe_value(atmosphere.altitude_m) / 9144.0);
    const double high_speed_blend = clamp((mach - 0.9) / 0.5, 0.0, 1.0);
    const double drag_scale = 1.0
        - clamp(options_.high_speed_zero_lift_drag_reduction, 0.0, 0.25)
            * high_speed_blend * high_speed_blend * (3.0 - 2.0 * high_speed_blend);
    double drag_coefficient = cd0 * transonic_factor * drag_scale
        + ref::speedbrake_drag * (input.speedbrake ? 1.2 : 0.0)
        + ref::flap_drag * flap_position_
        + ref::gear_drag * (input.gear_down ? 1.0 : 0.0)
        + sample_alpha_drag(alpha_deg);
    if (input.gear_down) {
        const double speed_factor = std::pow(ias / 100.0, 2.0);
        drag_coefficient += 0.005 * speed_factor;
    }
    if (on_ground && input.wheel_brake && ias_knots > 30.0) {
        drag_coefficient += 0.60;
    }

    output_.lift_coefficient = lift_coefficient;
    output_.drag_coefficient = drag_coefficient;
    output_.side_force_coefficient = tail_side_coefficient;

    const double aos_effect = std::abs(body.sideslip_rad) < 0.05
        ? 0.0 : std::sin(body.sideslip_rad / 2.0);
    const double drag_direction = std::abs(alpha_deg) < 100.0 ? 1.0 : -1.0;
    const double x_offset = alpha_deg <= 3.5 ? -0.8
        : alpha_deg >= 20.0 ? -0.5
        : -0.8 + ((alpha_deg - 3.5) / (20.0 - 3.5)) * 0.3;

    const Vec3 left_wing_position{
        mass.center_of_mass_m.x + x_offset,
        mass.center_of_mass_m.y + 0.5,
        -ref::wing_span_m / 2.0};
    const Vec3 right_wing_position{
        mass.center_of_mass_m.x + x_offset,
        mass.center_of_mass_m.y + 0.5,
        ref::wing_span_m / 2.0};
    const Vec3 left_wing_force{
        -drag_coefficient * drag_direction * (-aos_effect + 1.0) * q
            * (ref::wing_area_m2 / 2.0),
        lift_coefficient * (-aos_effect / 2.0 + 1.0) * q
            * (ref::wing_area_m2 / 2.0),
        tail_side_coefficient * q * (ref::wing_area_m2 / 2.0)};
    const Vec3 right_wing_force{
        -drag_coefficient * drag_direction * (aos_effect + 1.0) * q
            * (ref::wing_area_m2 / 2.0),
        lift_coefficient * (aos_effect / 2.0 + 1.0) * q
            * (ref::wing_area_m2 / 2.0),
        -tail_side_coefficient * q * (ref::wing_area_m2 / 2.0)};
    add_force(output_, mass, left_wing_force, left_wing_position);
    add_force(output_, mass, right_wing_force, right_wing_position);

    const Vec3 tail_position{
        mass.center_of_mass_m.x - 0.9, mass.center_of_mass_m.y, 0.0};
    const Vec3 tail_force{
        -tail_side_coefficient * std::sin(body.angle_of_attack_rad)
            * (ref::wing_area_m2 / 2.0) * q,
        0.0,
        -tail_side_coefficient * std::cos(body.angle_of_attack_rad)
            * (ref::wing_area_m2 / 2.0) * q};
    add_force(output_, mass, tail_force, tail_position);

    const double elevon_force_magnitude = q * ref::wing_area_m2 * 0.20;
    double elevon_alpha_scale = 1.0;
    if (std::abs(alpha_deg) > 30.0) {
        elevon_alpha_scale = clamp(1.0
            - 0.9 * (std::abs(alpha_deg) - 30.0) / 30.0, 0.1, 1.0);
    }
    const Vec3 left_elevon_position{
        -ref::length_m / 2.0, mass.center_of_mass_m.y,
        -ref::wing_span_m * 0.25};
    const Vec3 right_elevon_position{
        -ref::length_m / 2.0, mass.center_of_mass_m.y,
        ref::wing_span_m * 0.25};
    add_force(output_, mass,
        {0.0, left_elevon_rad_ * elevon_force_magnitude * elevon_alpha_scale, 0.0},
        left_elevon_position);
    add_force(output_, mass,
        {0.0, right_elevon_rad_ * elevon_force_magnitude * elevon_alpha_scale, 0.0},
        right_elevon_position);

    const double aileron_deflection_rad = roll_command * 30.0 * ref::deg_to_rad;
    const Vec3 left_aileron_position{
        mass.center_of_mass_m.x, mass.center_of_mass_m.y, -ref::wing_span_m * 0.5};
    const Vec3 right_aileron_position{
        mass.center_of_mass_m.x, mass.center_of_mass_m.y, ref::wing_span_m * 0.5};
    add_force(output_, mass,
        {0.0, aileron_deflection_rad * q * ref::wing_area_m2 * 0.25, 0.0},
        left_aileron_position);
    add_force(output_, mass,
        {0.0, -aileron_deflection_rad * q * ref::wing_area_m2 * 0.25, 0.0},
        right_aileron_position);

    const double rudder_deflection_rad = yaw_command
        * (25.0 + (mach < 0.5 ? 5.0 : 0.0)) * ref::deg_to_rad;
    const Vec3 rudder_position{-ref::length_m / 2.0, ref::height_m / 2.0, 0.0};
    add_force(output_, mass,
        {0.0, 0.0, rudder_deflection_rad * q * ref::wing_area_m2 * 0.30},
        rudder_position);

    // No hidden thrust vectoring in the initial F-23 candidate.
    const Vec3 left_engine_position{-4.793, 0.0, -0.716};
    const Vec3 right_engine_position{-4.793, 0.0, 0.716};
    add_force(output_, mass, {output_.engines[0].thrust_n, 0.0, 0.0}, left_engine_position);
    add_force(output_, mass, {output_.engines[1].thrust_n, 0.0, 0.0}, right_engine_position);

    // Reference-style high-order roll-rate and yaw/sideslip damping.
    const double hard_roll_ratio = std::abs(body.angular_rate_rad_s.x)
        / std::max(0.1, roll_rate_limit + 0.1);
    const double amplified_ratio = std::pow(clamp(hard_roll_ratio, 0.0001, 2.0), 6.0);
    const double dynamic_pressure_term = 2.0 * q + 50000.0;
    const double roll_rate_limiter = -body.angular_rate_rad_s.x
        * clamp(amplified_ratio * dynamic_pressure_term, -1.0e7, 1.0e7);
    output_.moment_n_m.x += roll_rate_limiter;
    // The artificial beta correction is an airborne handling aid. Its fixed
    // gain otherwise turns a parked aircraft into a crosswind despite brakes.
    // Keep rate damping and physical aerodynamic forces; let native tires/NWS
    // own ground steering. Restore beta correction smoothly after liftoff.
    airborne_yaw_time_s_ = on_ground ? 0.0 : std::min(0.5, airborne_yaw_time_s_ + dt);
    const auto smooth = [](double x) { return x * x * (3.0 - 2.0 * x); };
    const double yaw_airborne_blend = smooth(airborne_yaw_time_s_ / 0.5)
        * smooth(clamp((ias_knots - 10.0) / 40.0, 0.0, 1.0));
    // The fixed rate term is also a handling aid. At taxi speed it opposed
    // native nosewheel turns. Keep 10 percent on the wheels and restore it
    // between 30 and 70 KIAS, so takeoff and landing rolls keep it.
    const double ground_rate_blend = on_ground
        ? 0.1 + 0.9 * smooth(clamp((ias_knots - 30.0) / 40.0, 0.0, 1.0)) : 1.0;
    output_.moment_n_m.y += -(body.angular_rate_rad_s.y
        + yaw_airborne_blend * body.sideslip_rad) * (q + 50000.0 * ground_rate_blend);

    output_.shake_amplitude = 0.0;
    if (input.speedbrake) { output_.shake_amplitude += clamp((ref::speedbrake_drag + 1.0) * mach, 0.0, 2.0) / 6.0; }
    if (!on_ground) {
        if (std::abs(alpha_deg) > 45.0) {
            output_.shake_amplitude += (std::abs(alpha_deg) - 45.0) / 120.0;
        }
        if (std::abs(beta_deg) > 10.0) {
            output_.shake_amplitude += (std::abs(beta_deg) - 10.0) / 100.0;
        }
        if (std::abs(body.normal_load_g) > 9.5) {
            output_.shake_amplitude += (std::abs(body.normal_load_g) - 9.5) / 100.0;
        }
        if (mach > 2.31) { output_.shake_amplitude += (mach - 2.31) / 2.0; }
    }
    output_.shake_amplitude = clamp(output_.shake_amplitude, 0.0, 1.0);

    return output_;
}

} // namespace f23b::performance
