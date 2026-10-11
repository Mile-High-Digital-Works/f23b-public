// SPDX-License-Identifier: GPL-3.0-or-later
// Project interface for the recorded-data core and the native-system bridge.
// Units and axes follow the DCS SDK; no engine simulation is defined here.
#pragma once
#include <array>
#include <cstddef>
namespace f23b::performance {
struct Vec3 { double x{},y{},z{}; };
struct Atmosphere { double density_kg_m3{1.225},speed_of_sound_m_s{340.294},altitude_m{}; };
struct MassState {
    double mass_kg{19300};
    Vec3 center_of_mass_m{};
    Vec3 inertia_kg_m2{31087,240989,168771};
};
struct BodyState {
    Vec3 velocity_body_m_s{},wind_body_m_s{},angular_rate_rad_s{};
    double angle_of_attack_rad{},sideslip_rad{},roll_rad{},normal_load_g{1},altitude_agl_m{};
    bool weight_on_wheels{};
};
struct PilotInput {
    double pitch{},roll{},yaw{},throttle_left{},throttle_right{};
    bool analog_pitch{true},analog_roll{true},analog_yaw{true};
    bool engine_left_running{},engine_right_running{};
    bool gear_down{},speedbrake{},landing_flaps{},wheel_brake{};
};
struct EngineOutput { double thrust_n{},fuel_flow_kg_s{};bool combustion{}; };
struct ControlOutput {
    double pitch_command{},roll_command{},yaw_command{};
    double left_elevon_rad{},right_elevon_rad{},left_aileron{},right_aileron{},left_rudder{},right_rudder{};
    double flap{},leading_edge_flap{},angle_of_attack_limit_deg{},roll_rate_limit_rad_s{};
    bool g_limiter_active{},alpha_limiter_active{};
};
struct Output {
    Vec3 force_n{},moment_n_m{};
    std::array<EngineOutput,2> engines{};
    ControlOutput controls{};
    double airspeed_m_s{},indicated_airspeed_m_s{},mach{},dynamic_pressure_pa{};
    double lift_coefficient{},drag_coefficient{},side_force_coefficient{},shake_amplitude{},fuel_flow_kg_s{};
};
class Model {
public:
    Model() noexcept { reset(); }
    void reset() noexcept;
    Output step(double,const Atmosphere&,const MassState&,const BodyState&,const PilotInput&) noexcept;
private:
    Output output_{};
    double last_pitch_command_{},last_roll_command_{},last_yaw_command_{},flap_position_{};
    double airborne_yaw_time_s_{.5};
    bool low_speed_recovery_{};
};
}
