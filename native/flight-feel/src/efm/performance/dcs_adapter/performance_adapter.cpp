// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Thin DCS EFM adapter for the F-23B Grinnelli-v2.1 flight-feel candidate.
 *
 * This binary owns rigid-body forces/moments, propulsion physics, fuel and the
 * standard flight-control/engine draw channels only. It deliberately does not
 * create or publish radar, SMS, EW, MIDS, JHMCS, cockpit, mission-computer or
 * weapon state. Those remain owned by the installed F/A-18C cockpit context.
 *
 * Copyright (C) 2026 F-23B project contributors.
 */
#include "f23b/performance/engine_mapping.hpp"
#include "f23b/performance/performance_model.hpp"
#include "f23b/performance/grinnelli_v21_reference.hpp"

#include <FM/wHumanCustomPhysicsAPI.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

#if defined(_WIN32)
#define F23B_PERF_EXPORT extern "C" __declspec(dllexport)
#else
#define F23B_PERF_EXPORT extern "C" __attribute__((visibility("default")))
#endif

namespace perf = f23b::performance;

namespace {

constexpr int kPitchAxis = 2001;
constexpr int kRollAxis = 2002;
constexpr int kYawAxis = 2003;
constexpr int kThrottleAxis = 2004;
constexpr int kThrottleLeftAxis = 2005;
constexpr int kThrottleRightAxis = 2006;
constexpr int kPitchDown = 193;
constexpr int kPitchDownStop = 194;
constexpr int kPitchUp = 195;
constexpr int kPitchUpStop = 196;
constexpr int kRollLeft = 197;
constexpr int kRollLeftStop = 198;
constexpr int kRollRight = 199;
constexpr int kRollRightStop = 200;
constexpr int kRudderLeft = 201;
constexpr int kRudderLeftStop = 202;
constexpr int kRudderRight = 203;
constexpr int kRudderRightStop = 204;
constexpr int kEnginesOn = 309;
constexpr int kEnginesOff = 310;
constexpr int kLeftEngineOn = 311;
constexpr int kRightEngineOn = 312;
constexpr int kLeftEngineOff = 313;
constexpr int kRightEngineOff = 314;
constexpr int kGearToggle = 68;
constexpr int kGearUp = 430;
constexpr int kGearDown = 431;
constexpr int kWheelBrakeOn = 74;
constexpr int kWheelBrakeOff = 75;
constexpr int kSpeedbrakeToggle = 73;
constexpr int kSpeedbrakeOn = 147;
constexpr int kSpeedbrakeOff = 148;
constexpr int kFlapsToggle = 72;
constexpr int kFlapsUp = 145;
constexpr int kFlapsDown = 146;
constexpr int kThrottleIncrease = 1032;
constexpr int kThrottleDecrease = 1033;
constexpr int kThrottleStop = 1034;

constexpr double kGravity = 9.80665;
constexpr double kMaximumInternalFuelKg = 4900.0;
constexpr double kUsableFuelThresholdKg = 0.5;
constexpr double kGearTravelRatePerS = 0.35;
constexpr double kKeyboardThrottleRatePerS = 0.20;
constexpr double kCabinMinimumPressurePa = 75'000.0;
constexpr double kCabinMaximumOverpressurePa = 55'000.0;

struct RuntimeState {
    perf::Model model{};
    perf::Atmosphere atmosphere{};
    perf::MassState mass{};
    perf::BodyState body{};
    perf::PilotInput input{};
    perf::Output output{};
    perf::Vec3 world_velocity{};
    perf::Vec3 world_position{};
    double ambient_pressure_pa{101'325.0};
    double internal_fuel_kg{kMaximumInternalFuelKg};
    double pending_fuel_burn_kg{};
    double gear_position{};
    double throttle_left{};
    double throttle_right{};
    double keyboard_throttle_rate{};
    std::array<double, 3> suspension_compression{};
};

RuntimeState runtime{};

[[nodiscard]] double bounded(double value, double low, double high) noexcept {
    if (!std::isfinite(value)) { return low; }
    return std::clamp(value, low, high);
}

[[nodiscard]] double approach(double current, double target, double maximum_delta) noexcept {
    const double delta = target - current;
    return current + bounded(delta, -std::abs(maximum_delta), std::abs(maximum_delta));
}

[[nodiscard]] double decode_throttle_axis(float value) noexcept {
    // DCS aircraft axes use -1 at the forward stop and +1 at idle.
    return bounded(0.5 * (-static_cast<double>(value) + 1.0), 0.0, 1.0);
}

void set_combined_throttle(double value) noexcept {
    runtime.input.throttle_left = runtime.input.throttle_right = bounded(value, 0.0, 1.0);
    runtime.throttle_left = runtime.input.throttle_left;
    runtime.throttle_right = runtime.input.throttle_right;
}

void reset_common(bool engines_running, bool gear_down) noexcept {
    runtime.model.reset();
    runtime.output = {};
    runtime.input = {};
    runtime.input.engine_left_running = engines_running;
    runtime.input.engine_right_running = engines_running;
    runtime.input.gear_down = gear_down;
    runtime.input.analog_pitch = true;
    runtime.input.analog_roll = true;
    runtime.input.analog_yaw = true;
    runtime.throttle_left = 0.0;
    runtime.throttle_right = 0.0;
    runtime.keyboard_throttle_rate = 0.0;
    runtime.gear_position = gear_down ? 1.0 : 0.0;
    runtime.pending_fuel_burn_kg = 0.0;
    runtime.suspension_compression = {};
}

[[nodiscard]] double cabin_overpressure_pa() noexcept {
    if (!std::isfinite(runtime.ambient_pressure_pa)) { return 0.0; }
    return bounded(kCabinMinimumPressurePa - runtime.ambient_pressure_pa,
        0.0, kCabinMaximumOverpressurePa);
}

[[nodiscard]] double engine_parameter(unsigned index, unsigned base,
    const perf::EngineOutput& engine) noexcept {
    const unsigned offset = index - base;
    using Field = perf::EngineParameterField;
    switch (offset) {
    case ED_FM_ENGINE_0_RPM:
        return perf::engine_parameter_value(Field::Rpm, engine, runtime.output.mach);
    case ED_FM_ENGINE_0_RELATED_RPM:
        return perf::engine_parameter_value(Field::RelatedRpm, engine, runtime.output.mach);
    case ED_FM_ENGINE_0_CORE_RPM:
        return perf::engine_parameter_value(Field::CoreRpm, engine, runtime.output.mach);
    case ED_FM_ENGINE_0_CORE_RELATED_RPM:
        return perf::engine_parameter_value(Field::CoreRelatedRpm, engine, runtime.output.mach);
    case ED_FM_ENGINE_0_STARTER_RELATED_RPM:
        return perf::engine_parameter_value(Field::StarterRelatedRpm, engine,
            runtime.output.mach, 0.0);
    case ED_FM_ENGINE_0_THRUST:
        return perf::engine_parameter_value(Field::Thrust, engine, runtime.output.mach);
    case ED_FM_ENGINE_0_RELATED_THRUST:
        return perf::engine_parameter_value(Field::RelatedThrust, engine, runtime.output.mach);
    case ED_FM_ENGINE_0_CORE_THRUST:
        return perf::engine_parameter_value(Field::CoreThrust, engine, runtime.output.mach);
    case ED_FM_ENGINE_0_CORE_RELATED_THRUST:
        return perf::engine_parameter_value(Field::CoreRelatedThrust, engine, runtime.output.mach);
    case ED_FM_ENGINE_0_FUEL_FLOW:
        return perf::engine_parameter_value(Field::FuelFlow, engine, runtime.output.mach);
    case ED_FM_ENGINE_0_COMBUSTION:
        return perf::engine_parameter_value(Field::Combustion, engine, runtime.output.mach);
    case ED_FM_ENGINE_0_TEMPERATURE:
        return perf::engine_parameter_value(Field::Temperature, engine, runtime.output.mach);
    case ED_FM_ENGINE_0_GAIN:
        return perf::engine_parameter_value(Field::Gain, engine, runtime.output.mach);
    case ED_FM_ENGINE_0_FLOW_SPEED:
        return perf::engine_parameter_value(Field::FlowSpeed, engine, runtime.output.mach);
    default:
        return 0.0;
    }
}

template <typename Writer>
void write_control_arguments(std::size_t count, Writer&& write) noexcept {
    const auto& controls = runtime.output.controls;
    const auto set = [&](std::size_t argument, double value) {
        if (argument < count) { write(argument, static_cast<float>(value)); }
    };

    // Hornet-standard exterior controls used by the accepted F-23 EDM.
    set(9, controls.flap);
    set(10, controls.flap);
    set(11, controls.left_aileron);
    set(12, controls.right_aileron);
    set(13, controls.leading_edge_flap);
    set(14, controls.leading_edge_flap);
    // Hornet/Grinnelli convention: argument 15 right stabilator, 16 left.
    constexpr double visual_stabilator_reference_rad = 18.0
        * f23b::performance::grinnelli_v21::deg_to_rad;
    set(15, bounded(controls.right_elevon_rad / -visual_stabilator_reference_rad,
        -1.0, 1.0));
    set(16, bounded(controls.left_elevon_rad / -visual_stabilator_reference_rad,
        -1.0, 1.0));
    set(17, controls.left_rudder);
    set(18, controls.right_rudder);
}

template <typename Writer>
void write_draw_arguments(std::size_t count, Writer&& write) noexcept {
    write_control_arguments(count, write);
    const auto& controls = runtime.output.controls;
    const auto& left_engine = runtime.output.engines[0];
    const auto& right_engine = runtime.output.engines[1];
    const auto set = [&](std::size_t argument, double value) {
        if (argument < count) { write(argument, static_cast<float>(value)); }
    };

    // Final Hornet visual contract: left burner/nozzle = 29/90, right = 28/89.
    set(29, perf::burner_stage_argument(left_engine));
    set(28, perf::burner_stage_argument(right_engine));
    set(90, perf::raw_nozzle_argument(left_engine));
    set(89, perf::raw_nozzle_argument(right_engine));

    // The cockpit-owned exterior adapter remains the only writer of 1009-1012
    // (main bay, gun door, filtered left/right SERN wedges).
}

} // namespace

F23B_PERF_EXPORT void ed_fm_add_local_force(double& x, double& y, double& z,
    double& pos_x, double& pos_y, double& pos_z) {
    x = runtime.output.force_n.x;
    y = runtime.output.force_n.y;
    z = runtime.output.force_n.z;
    pos_x = runtime.mass.center_of_mass_m.x;
    pos_y = runtime.mass.center_of_mass_m.y;
    pos_z = runtime.mass.center_of_mass_m.z;
}

F23B_PERF_EXPORT void ed_fm_add_global_force(double& x, double& y, double& z,
    double& pos_x, double& pos_y, double& pos_z) {
    x = y = z = 0.0;
    pos_x = runtime.mass.center_of_mass_m.x;
    pos_y = runtime.mass.center_of_mass_m.y;
    pos_z = runtime.mass.center_of_mass_m.z;
}

F23B_PERF_EXPORT bool ed_fm_add_local_force_component(double&, double&, double&,
    double&, double&, double&) { return false; }
F23B_PERF_EXPORT bool ed_fm_add_global_force_component(double&, double&, double&,
    double&, double&, double&) { return false; }

F23B_PERF_EXPORT void ed_fm_add_local_moment(double& x, double& y, double& z) {
    x = runtime.output.moment_n_m.x;
    y = runtime.output.moment_n_m.y;
    z = runtime.output.moment_n_m.z;
}
F23B_PERF_EXPORT void ed_fm_add_global_moment(double& x, double& y, double& z) {
    x = y = z = 0.0;
}
F23B_PERF_EXPORT bool ed_fm_add_local_moment_component(double&, double&, double&) {
    return false;
}
F23B_PERF_EXPORT bool ed_fm_add_global_moment_component(double&, double&, double&) {
    return false;
}

F23B_PERF_EXPORT void ed_fm_simulate(double dt) {
    const double safe_dt = bounded(dt, 0.0, 0.05);
    if (runtime.keyboard_throttle_rate != 0.0) {
        set_combined_throttle(0.5 * (runtime.throttle_left + runtime.throttle_right)
            + runtime.keyboard_throttle_rate * kKeyboardThrottleRatePerS * safe_dt);
    }
    runtime.gear_position = approach(runtime.gear_position,
        runtime.input.gear_down ? 1.0 : 0.0, kGearTravelRatePerS * safe_dt);

    const bool has_fuel = runtime.internal_fuel_kg > kUsableFuelThresholdKg;
    auto effective_input = runtime.input;
    effective_input.engine_left_running &= has_fuel;
    effective_input.engine_right_running &= has_fuel;
    runtime.body.weight_on_wheels = std::any_of(
        runtime.suspension_compression.begin(), runtime.suspension_compression.end(),
        [](double compression) { return compression > 0.0; });
    runtime.output = runtime.model.step(safe_dt, runtime.atmosphere,
        runtime.mass, runtime.body, effective_input);

    const double fuel_burn = std::min(runtime.internal_fuel_kg,
        std::max(0.0, runtime.output.fuel_flow_kg_s) * safe_dt);
    runtime.internal_fuel_kg -= fuel_burn;
    runtime.pending_fuel_burn_kg += fuel_burn;
}

F23B_PERF_EXPORT void ed_fm_set_atmosphere(double altitude, double,
    double speed_of_sound, double density, double pressure,
    double wind_vx, double wind_vy, double wind_vz) {
    runtime.atmosphere.altitude_m = std::isfinite(altitude) ? altitude : 0.0;
    runtime.atmosphere.speed_of_sound_m_s = std::max(1.0,
        std::isfinite(speed_of_sound) ? speed_of_sound : 340.294);
    runtime.atmosphere.density_kg_m3 = std::max(0.0,
        std::isfinite(density) ? density : 1.225);
    runtime.ambient_pressure_pa = std::isfinite(pressure) ? pressure : 101'325.0;
    runtime.body.wind_body_m_s = {wind_vx, wind_vy, wind_vz};
}

F23B_PERF_EXPORT void ed_fm_set_surface(double height, double height_with_objects,
    unsigned, double, double, double) {
    const double surface = std::isfinite(height) ? height : 0.0;
    const double objects = std::isfinite(height_with_objects) ? height_with_objects : 0.0;
    runtime.body.altitude_agl_m = runtime.atmosphere.altitude_m
        - std::max(surface, objects);
}

F23B_PERF_EXPORT void ed_fm_set_current_mass_state(double mass,
    double center_x, double center_y, double center_z,
    double inertia_x, double inertia_y, double inertia_z) {
    if (std::isfinite(mass) && mass > 0.0) { runtime.mass.mass_kg = mass; }
    runtime.mass.center_of_mass_m = {
        std::isfinite(center_x) ? center_x : 0.0,
        std::isfinite(center_y) ? center_y : 0.0,
        std::isfinite(center_z) ? center_z : 0.0};
    runtime.mass.inertia_kg_m2 = {
        std::isfinite(inertia_x) && inertia_x > 0.0 ? inertia_x : 45'000.0,
        std::isfinite(inertia_y) && inertia_y > 0.0 ? inertia_y : 920'000.0,
        std::isfinite(inertia_z) && inertia_z > 0.0 ? inertia_z : 950'000.0};
}

F23B_PERF_EXPORT void ed_fm_set_current_state(double, double, double,
    double vx, double vy, double vz,
    double px, double py, double pz,
    double, double, double, double, double, double,
    double, double, double, double) {
    runtime.world_velocity = {vx, vy, vz};
    runtime.world_position = {px, py, pz};
}

F23B_PERF_EXPORT void ed_fm_set_current_state_body_axis(double ax, double ay,
    double az, double vx, double vy, double vz,
    double wind_vx, double wind_vy, double wind_vz,
    double, double, double, double omega_x, double omega_y, double omega_z,
    double, double pitch, double roll, double angle_of_attack, double sideslip) {
    runtime.body.velocity_body_m_s = {vx, vy, vz};
    runtime.body.wind_body_m_s = {wind_vx, wind_vy, wind_vz};
    runtime.body.angular_rate_rad_s = {omega_x, omega_y, omega_z};
    runtime.body.angle_of_attack_rad = std::isfinite(angle_of_attack)
        ? angle_of_attack : 0.0;
    runtime.body.sideslip_rad = std::isfinite(sideslip) ? sideslip : 0.0;
    runtime.body.roll_rad = std::isfinite(roll) ? roll : 0.0;
    // SDK supplies kinematic acceleration: subtract gravity projected into
    // body +Y. Preserve the sign for negative-g protection, including inverted flight.
    const double safe_pitch = std::isfinite(pitch) ? pitch : 0.0;
    runtime.body.normal_load_g = (std::isfinite(ay) ? ay : 0.0) / kGravity
        + std::cos(safe_pitch) * std::cos(runtime.body.roll_rad);
    (void)ax;
    (void)az;
}

F23B_PERF_EXPORT void ed_fm_set_command(int command, float value) {
    switch (command) {
    case kPitchAxis:
        runtime.input.pitch = bounded(value, -1.0, 1.0);
        runtime.input.analog_pitch = true;
        break;
    case kRollAxis:
        runtime.input.roll = bounded(value, -1.0, 1.0);
        runtime.input.analog_roll = true;
        break;
    case kYawAxis:
        runtime.input.yaw = bounded(-static_cast<double>(value), -1.0, 1.0);
        runtime.input.analog_yaw = true;
        break;
    case kThrottleAxis:
        set_combined_throttle(decode_throttle_axis(value));
        break;
    case kThrottleLeftAxis:
        runtime.throttle_left = decode_throttle_axis(value);
        runtime.input.throttle_left = runtime.throttle_left;
        runtime.input.throttle_right = runtime.throttle_right;
        break;
    case kThrottleRightAxis:
        runtime.throttle_right = decode_throttle_axis(value);
        runtime.input.throttle_left = runtime.throttle_left;
        runtime.input.throttle_right = runtime.throttle_right;
        break;
    case kPitchUp:
        runtime.input.pitch = 1.0;
        runtime.input.analog_pitch = false;
        break;
    case kPitchUpStop:
    case kPitchDownStop:
        runtime.input.pitch = 0.0;
        runtime.input.analog_pitch = false;
        break;
    case kPitchDown:
        runtime.input.pitch = -1.0;
        runtime.input.analog_pitch = false;
        break;
    case kRollLeft:
        runtime.input.roll = -1.0;
        runtime.input.analog_roll = false;
        break;
    case kRollRight:
        runtime.input.roll = 1.0;
        runtime.input.analog_roll = false;
        break;
    case kRollLeftStop:
    case kRollRightStop:
        runtime.input.roll = 0.0;
        runtime.input.analog_roll = false;
        break;
    case kRudderLeft:
        runtime.input.yaw = 1.0;
        runtime.input.analog_yaw = false;
        break;
    case kRudderRight:
        runtime.input.yaw = -1.0;
        runtime.input.analog_yaw = false;
        break;
    case kRudderLeftStop:
    case kRudderRightStop:
        runtime.input.yaw = 0.0;
        runtime.input.analog_yaw = false;
        break;
    case kEnginesOn:
        runtime.input.engine_left_running = true;
        runtime.input.engine_right_running = true;
        break;
    case kEnginesOff:
        runtime.input.engine_left_running = false;
        runtime.input.engine_right_running = false;
        break;
    case kLeftEngineOn: runtime.input.engine_left_running = true; break;
    case kRightEngineOn: runtime.input.engine_right_running = true; break;
    case kLeftEngineOff: runtime.input.engine_left_running = false; break;
    case kRightEngineOff: runtime.input.engine_right_running = false; break;
    case kGearToggle:
        if (value > 0.5F) { runtime.input.gear_down = !runtime.input.gear_down; }
        break;
    case kGearUp: runtime.input.gear_down = false; break;
    case kGearDown: runtime.input.gear_down = true; break;
    case kWheelBrakeOn: runtime.input.wheel_brake = true; break;
    case kWheelBrakeOff: runtime.input.wheel_brake = false; break;
    case kSpeedbrakeToggle:
        if (value > 0.5F) { runtime.input.speedbrake = !runtime.input.speedbrake; }
        break;
    case kSpeedbrakeOn: runtime.input.speedbrake = true; break;
    case kSpeedbrakeOff: runtime.input.speedbrake = false; break;
    case kFlapsToggle:
        if (value > 0.5F) { runtime.input.landing_flaps = !runtime.input.landing_flaps; }
        break;
    case kFlapsUp: runtime.input.landing_flaps = false; break;
    case kFlapsDown: runtime.input.landing_flaps = true; break;
    case kThrottleIncrease: runtime.keyboard_throttle_rate = 1.0; break;
    case kThrottleDecrease: runtime.keyboard_throttle_rate = -1.0; break;
    case kThrottleStop: runtime.keyboard_throttle_rate = 0.0; break;
    default: break;
    }
}

F23B_PERF_EXPORT bool ed_fm_change_mass(double& delta_mass, double& x,
    double& y, double& z, double& moi_x, double& moi_y, double& moi_z) {
    if (!(runtime.pending_fuel_burn_kg > 0.0)) { return false; }
    delta_mass = runtime.pending_fuel_burn_kg;
    runtime.pending_fuel_burn_kg = 0.0;
    x = runtime.mass.center_of_mass_m.x;
    y = runtime.mass.center_of_mass_m.y;
    z = runtime.mass.center_of_mass_m.z;
    moi_x = moi_y = moi_z = 0.0;
    return true;
}

F23B_PERF_EXPORT void ed_fm_set_internal_fuel(double fuel) {
    runtime.internal_fuel_kg = bounded(fuel, 0.0, kMaximumInternalFuelKg);
}
F23B_PERF_EXPORT double ed_fm_get_internal_fuel() {
    return runtime.internal_fuel_kg;
}
F23B_PERF_EXPORT void ed_fm_refueling_add_fuel(double fuel) {
    runtime.internal_fuel_kg = bounded(runtime.internal_fuel_kg + fuel,
        0.0, kMaximumInternalFuelKg);
}
F23B_PERF_EXPORT void ed_fm_set_external_fuel(int, double, double, double, double) {}
F23B_PERF_EXPORT double ed_fm_get_external_fuel() { return 0.0; }

F23B_PERF_EXPORT void ed_fm_set_draw_args(EdDrawArgument* arguments,
    std::size_t count) {
    if (arguments == nullptr) { return; }
    write_draw_arguments(count,
        [&](std::size_t index, float value) { arguments[index].f = value; });
}
F23B_PERF_EXPORT void ed_fm_set_draw_args_v2(float* arguments,
    std::size_t count) {
    if (arguments == nullptr) { return; }
    write_draw_arguments(count,
        [&](std::size_t index, float value) { arguments[index] = value; });
}
F23B_PERF_EXPORT void ed_fm_set_fc3_cockpit_draw_args(EdDrawArgument*,
    std::size_t) {}
F23B_PERF_EXPORT void ed_fm_set_fc3_cockpit_draw_args_v2(float*,
    std::size_t) {}

F23B_PERF_EXPORT double ed_fm_get_shake_amplitude() {
    return runtime.output.shake_amplitude;
}
F23B_PERF_EXPORT void ed_fm_configure(const char*) {}
F23B_PERF_EXPORT void ed_fm_release() { runtime = RuntimeState{}; }

F23B_PERF_EXPORT double ed_fm_get_param(unsigned index) {
    if (index >= ED_FM_ENGINE_1_RPM && index < ED_FM_ENGINE_2_RPM) {
        return engine_parameter(index, ED_FM_ENGINE_1_RPM, runtime.output.engines[0]);
    }
    if (index >= ED_FM_ENGINE_2_RPM && index < ED_FM_ENGINE_3_RPM) {
        return engine_parameter(index, ED_FM_ENGINE_2_RPM, runtime.output.engines[1]);
    }
    switch (index) {
    case ED_FM_FC3_STICK_PITCH: return runtime.input.pitch;
    case ED_FM_FC3_STICK_ROLL: return runtime.input.roll;
    case ED_FM_FC3_RUDDER_PEDALS: return runtime.input.yaw;
    case ED_FM_FC3_THROTTLE_LEFT: return runtime.throttle_left;
    case ED_FM_FC3_THROTTLE_RIGHT: return runtime.throttle_right;
    case ED_FM_SUSPENSION_0_RELATIVE_BRAKE_MOMENT: return 0.0;
    case ED_FM_SUSPENSION_1_RELATIVE_BRAKE_MOMENT:
    case ED_FM_SUSPENSION_2_RELATIVE_BRAKE_MOMENT:
        return runtime.input.wheel_brake ? 1.0 : 0.0;
    case ED_FM_SUSPENSION_0_GEAR_POST_STATE:
    case ED_FM_SUSPENSION_1_GEAR_POST_STATE:
    case ED_FM_SUSPENSION_2_GEAR_POST_STATE:
        return runtime.gear_position;
    case ED_FM_SUSPENSION_0_UP_LOCK:
    case ED_FM_SUSPENSION_1_UP_LOCK:
    case ED_FM_SUSPENSION_2_UP_LOCK:
        return runtime.gear_position <= 0.001 ? 1.0 : 0.0;
    case ED_FM_SUSPENSION_0_DOWN_LOCK:
    case ED_FM_SUSPENSION_1_DOWN_LOCK:
    case ED_FM_SUSPENSION_2_DOWN_LOCK:
        return runtime.gear_position >= 0.999 ? 1.0 : 0.0;
    case ED_FM_SUSPENSION_0_WHEEL_YAW:
        return runtime.input.yaw * (30.0 * 3.14159265358979323846 / 180.0);
    case ED_FM_SUSPENSION_1_WHEEL_YAW:
    case ED_FM_SUSPENSION_2_WHEEL_YAW:
    case ED_FM_SUSPENSION_0_WHEEL_SELF_ATTITUDE:
    case ED_FM_SUSPENSION_1_WHEEL_SELF_ATTITUDE:
    case ED_FM_SUSPENSION_2_WHEEL_SELF_ATTITUDE:
        return 0.0;
    case ED_FM_COCKPIT_PRESSURIZATION_OVER_EXTERNAL:
        return cabin_overpressure_pa();
    case ED_FM_FLOW_VELOCITY:
        return std::max(
            perf::engine_parameter_value(perf::EngineParameterField::FlowSpeed,
                runtime.output.engines[0], runtime.output.mach),
            perf::engine_parameter_value(perf::EngineParameterField::FlowSpeed,
                runtime.output.engines[1], runtime.output.mach));
    default: return 0.0;
    }
}

F23B_PERF_EXPORT void ed_fm_cold_start() { reset_common(false, true); }
F23B_PERF_EXPORT void ed_fm_hot_start() { reset_common(true, true); }
F23B_PERF_EXPORT void ed_fm_hot_start_in_air() { reset_common(true, false); }

F23B_PERF_EXPORT void ed_fm_suspension_feedback(int index,
    const ed_fm_suspension_info* info) {
    if (index < 0 || index >= 3 || info == nullptr
        || !std::isfinite(info->struct_compression)) {
        return;
    }
    runtime.suspension_compression[static_cast<std::size_t>(index)] =
        info->struct_compression;
}
