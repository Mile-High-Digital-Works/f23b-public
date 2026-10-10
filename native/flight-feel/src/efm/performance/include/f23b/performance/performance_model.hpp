// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * F-23B Grinnelli-v2.1 flight-feel model.
 * Modified and independently structured for the F-23B project, 2026-09-03.
 * See docs/provenance/GRINNELLI_V2_1_PERFORMANCE_REFERENCE.md.
 */
#pragma once

#include <array>
#include <cstddef>

namespace f23b::performance {

struct Vec3 {
    double x{};
    double y{};
    double z{};
};

struct Atmosphere {
    double density_kg_m3{1.225};
    double speed_of_sound_m_s{340.294};
    double altitude_m{};
};

struct MassState {
    double mass_kg{16651.0};
    Vec3 center_of_mass_m{};
    // DCS body-axis tensor: roll X, yaw Y, pitch Z.
    Vec3 inertia_kg_m2{45000.0, 920000.0, 950000.0};
};

struct BodyState {
    Vec3 velocity_body_m_s{};
    Vec3 wind_body_m_s{};
    Vec3 angular_rate_rad_s{};
    double angle_of_attack_rad{};
    double sideslip_rad{};
    double roll_rad{};
    double normal_load_g{1.0};
    double altitude_agl_m{1000.0};
    bool weight_on_wheels{};
};

struct PilotInput {
    double pitch{};
    double roll{};
    double yaw{};
    double throttle_left{}; // 0..1, independent engine commands
    double throttle_right{};
    bool analog_pitch{true};
    bool analog_roll{true};
    bool analog_yaw{true};
    bool engine_left_running{true};
    bool engine_right_running{true};
    bool gear_down{};
    bool speedbrake{};
    bool landing_flaps{};
    bool wheel_brake{};
};

struct Options {
    bool enable_virtual_thrust_vectoring{false};
    // The accepted F-23 exterior uses continuous burner and nozzle state.
    bool continuous_engine_visual_state{true};
    // Optional F-23 tune: smooth onset from Mach 0.9 to 1.4. Only the clean
    // zero-lift/wave-drag term changes; reference behavior defaults to zero.
    double high_speed_zero_lift_drag_reduction{};
};

struct EngineOutput {
    double throttle_state{};       // upstream-style 0.67..1.10 while running
    double spool_fraction{};       // normalized 0..1 for cockpit/visual consumers
    double afterburner_fraction{}; // continuous 0..1
    double thrust_n{};
    double fuel_flow_kg_s{};
    double rpm{};
    bool combustion{};
};

struct ControlOutput {
    double pitch_command{};
    double roll_command{};
    double yaw_command{};
    double left_elevon_rad{};
    double right_elevon_rad{};
    double left_aileron{};
    double right_aileron{};
    double left_rudder{};
    double right_rudder{};
    double flap{};
    double leading_edge_flap{};
    double angle_of_attack_limit_deg{};
    double roll_rate_limit_rad_s{};
    bool g_limiter_active{};
    bool alpha_limiter_active{};
};

struct Output {
    Vec3 force_n{};
    Vec3 moment_n_m{};
    std::array<EngineOutput, 2> engines{};
    ControlOutput controls{};
    double airspeed_m_s{};
    double indicated_airspeed_m_s{};
    double mach{};
    double dynamic_pressure_pa{};
    double lift_coefficient{};
    double drag_coefficient{};
    double side_force_coefficient{};
    double shake_amplitude{};
    double fuel_flow_kg_s{};
};

class Model {
public:
    explicit Model(Options options = {}) noexcept;

    void reset() noexcept;
    // Offline analysis helper: prime the internal engine actuator state at the
    // requested command without simulating spool time. The DCS adapter never
    // calls this; runtime response remains time-dependent.
    void prime_engines_for_offline_analysis(double throttle,
        bool left_running = true, bool right_running = true) noexcept;
    [[nodiscard]] Output step(double dt_s, const Atmosphere& atmosphere,
        const MassState& mass, const BodyState& body, const PilotInput& input) noexcept;

    [[nodiscard]] const Output& output() const noexcept { return output_; }
    [[nodiscard]] const Options& options() const noexcept { return options_; }

private:
    struct EngineState {
        double throttle_state{};
        double afterburner_fraction{};
    };

    Options options_{};
    Output output_{};
    std::array<EngineState, 2> engines_{};
    double smoothed_pitch_discrete_{};
    double smoothed_roll_discrete_{};
    double smoothed_yaw_discrete_{};
    double autotrim_command_{};
    double takeoff_trim_command_{};
    double beta_integral_{};
    double last_yaw_input_{};
    double last_pitch_command_{};
    double last_roll_command_{};
    double last_yaw_command_{};
    double left_elevon_rad_{};
    double right_elevon_rad_{};
    double flap_position_{};
    double leading_edge_flap_position_{};
    double simulation_time_s_{};
    double airborne_yaw_time_s_{0.5};
};

// Portable helpers used by offline scoring and native tests.
[[nodiscard]] double clamp(double value, double low, double high) noexcept;
[[nodiscard]] double interpolate(const double* xs, const double* ys,
    std::size_t count, double x) noexcept;
[[nodiscard]] double shaped_axis(double input) noexcept;
[[nodiscard]] double approach(double current, double target,
    double maximum_delta) noexcept;

} // namespace f23b::performance
