// SPDX-License-Identifier: GPL-3.0-or-later
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include "own_adapter.hpp"
#include "propulsion.hpp"
#include "steering.hpp"

// Only documented, typed SDK inputs are dispatched to both implementations.
// All other native callbacks remain unchanged in the compatibility experiment.
template<auto Own> struct Both;
template<class... Args, void(*Own)(Args...)>
struct Both<Own> {
    static inline void(*native)(Args...){};
    static void invoke(Args... args) { native(args...); Own(args...); }
};

namespace {
decltype(&own_ed_fm_configure) native_configure{};
decltype(&own_ed_fm_get_param) native_parameter{};
decltype(&own_ed_fm_simulate) native_simulate{};
decltype(&own_ed_fm_get_internal_fuel) native_fuel{};
decltype(&own_ed_fm_set_draw_args_v2) native_draw{};
decltype(&own_ed_fm_get_shake_amplitude) native_shake{};
std::array<float, 3> last_native_gear_draw{};
std::array<double, 3> contact_vertical_force{};

double ground_speed() {
    const auto& v = runtime.body.velocity_body_m_s;
    return std::hypot(v.x, v.z);
}

double parameter(unsigned index) {
    const double value = native_parameter(index);
    return index == ED_FM_SUSPENSION_0_WHEEL_YAW ? f23b::bridge::nose_wheel_yaw(value, ground_speed()) : value;
}

void configure_bridge(const char* path) {
    native_configure(path);
    own_ed_fm_configure(path);
    runtime.model = perf::Model{f23b::bridge::flight_options};
}

decltype(&own_ed_fm_suspension_feedback) native_suspension{};
void suspension_feedback(int index, const ed_fm_suspension_info* info) {
    if (info && index >= 0 && index < 3) contact_vertical_force[index] = info->acting_force[1];
    own_ed_fm_suspension_feedback(index, info);
    native_suspension(index, info);
}

void draw_native(float* arguments, std::size_t count) {
    native_draw(arguments, count);
    if (arguments && count > 5) last_native_gear_draw = {arguments[0], arguments[5], arguments[3]};
    // Argument 2 shows the nosewheel angle. Keep it equal to the steering angle
    // that DCS receives from parameter().
    const double native_yaw = native_parameter ? native_parameter(ED_FM_SUSPENSION_0_WHEEL_YAW) : 0.0;
    if (arguments && count > 2 && std::abs(native_yaw) > 1e-6) {
        const double shown = arguments[2] * f23b::bridge::nose_wheel_yaw(native_yaw, ground_speed()) / native_yaw;
        arguments[2] = static_cast<float>(std::clamp(shown, -1.0, 1.0));
    }
    // Read native flap input before presenting the controls that actually
    // generate the independent model's forces. Engines/gear/bays stay native.
    if (arguments && count > 10) runtime.input.landing_flaps = arguments[9] > 0.5f;
    if (arguments) write_control_arguments(count, [&](std::size_t index, float value) {
        arguments[index] = value;
    });
}

double shake_for_active_forces() {
    // Preserve native ground-contact feedback. In flight, buffet must follow
    // the model producing aerodynamic forces, not the unused Hornet FCS.
    return runtime.body.weight_on_wheels ? native_shake() : own_ed_fm_get_shake_amplitude();
}

void simulate_native_engines(double dt) {
    native_simulate(dt);
    runtime.internal_fuel_kg = std::max(0.0, native_fuel());
    runtime.input.gear_down = native_parameter(ED_FM_SUSPENSION_0_GEAR_POST_STATE) > 0.5;
    runtime.input.wheel_brake = native_parameter(ED_FM_SUSPENSION_1_RELATIVE_BRAKE_MOMENT) > 0.1;
    // Native engines exclusively own spool, combustion and fuel. The independent
    // core supplies aerodynamics/FCS only, so it must not burn fuel a second time.
    runtime.input.engine_left_running = false;
    runtime.input.engine_right_running = false;
    own_ed_fm_simulate(dt);
    std::array<double, 2> thrust{}, flow{};
    for (std::size_t i = 0; i < 2; ++i) {
        const auto base = i == 0 ? ED_FM_ENGINE_1_RPM : ED_FM_ENGINE_2_RPM;
        thrust[i] = std::max(0.0, native_parameter(base + (ED_FM_ENGINE_1_THRUST - ED_FM_ENGINE_1_RPM)));
        flow[i] = native_parameter(base + (ED_FM_ENGINE_1_FUEL_FLOW - ED_FM_ENGINE_1_RPM));
        const double related_rpm = native_parameter(base + (ED_FM_ENGINE_1_RELATED_RPM - ED_FM_ENGINE_1_RPM));
        f23b::bridge::add_native_engine(runtime.output, runtime.mass, i, thrust[i], flow[i], related_rpm);
    }
    static double time = 0.0, last = -1.0;
    time += dt;
    if (time - last >= 0.05) {
        last = time;
        wchar_t path[2048]{};
        const auto length = GetEnvironmentVariableW(L"F23B_FORCE_BRIDGE_STATE", path, 2048);
        if (length > 0 && length < 2048) {
            std::ofstream out(std::filesystem::path(path), std::ios::app);
            out << std::setprecision(12) << time << ',' << runtime.mass.mass_kg << ','
                << native_fuel() << ',' << thrust[0] << ',' << thrust[1] << ','
                << runtime.output.engines[0].thrust_n << ',' << runtime.output.engines[1].thrust_n << ','
                << flow[0] << ',' << flow[1] << ',' << runtime.output.force_n.x << ','
                << native_parameter(ED_FM_ENGINE_1_RELATED_RPM) << ','
                << native_parameter(ED_FM_ENGINE_2_RELATED_RPM) << '\n';
        }
        const auto mechanics_length = GetEnvironmentVariableW(L"F23B_FORCE_BRIDGE_MECHANICS", path, 2048);
        if (mechanics_length > 0 && mechanics_length < 2048) {
            std::ofstream out(std::filesystem::path(path), std::ios::app);
            out << std::setprecision(12) << time << ','
                << native_parameter(ED_FM_SUSPENSION_0_GEAR_POST_STATE) << ','
                << native_parameter(ED_FM_SUSPENSION_1_GEAR_POST_STATE) << ','
                << native_parameter(ED_FM_SUSPENSION_2_GEAR_POST_STATE) << ','
                << last_native_gear_draw[0] << ',' << last_native_gear_draw[1] << ',' << last_native_gear_draw[2] << ','
                << runtime.suspension_compression[0] << ',' << runtime.suspension_compression[1] << ',' << runtime.suspension_compression[2] << ','
                << contact_vertical_force[0] << ',' << contact_vertical_force[1] << ',' << contact_vertical_force[2] << ','
                << native_parameter(ED_FM_SUSPENSION_0_WHEEL_YAW) << '\n';
        }
        const auto ground_length = GetEnvironmentVariableW(L"F23B_FORCE_BRIDGE_GROUND", path, 2048);
        if (ground_length > 0 && ground_length < 2048) {
            std::ofstream out(std::filesystem::path(path), std::ios::app);
            out << std::setprecision(12) << time << ',' << runtime.body.weight_on_wheels << ','
                << runtime.body.sideslip_rad << ',' << runtime.body.angular_rate_rad_s.y << ','
                << runtime.output.moment_n_m.y << ',' << runtime.output.force_n.x << ','
                << native_parameter(ED_FM_SUSPENSION_1_RELATIVE_BRAKE_MOMENT) << ','
                << native_parameter(ED_FM_SUSPENSION_2_RELATIVE_BRAKE_MOMENT) << ','
                << contact_vertical_force[0] << ',' << contact_vertical_force[1] << ','
                << contact_vertical_force[2] << ','
                << native_parameter(ED_FM_SUSPENSION_0_WHEEL_YAW) << ','
                << thrust[0] << ',' << thrust[1] << '\n';
        }
    }
}
}

FARPROC f23b_select_callback(const char* name, FARPROC original) {
    if (std::strcmp(name, "ed_fm_get_shake_amplitude") == 0) {
        native_shake = reinterpret_cast<decltype(native_shake)>(original);
        return reinterpret_cast<FARPROC>(&shake_for_active_forces);
    }
    if (std::strcmp(name, "ed_fm_configure") == 0) {
        native_configure = reinterpret_cast<decltype(native_configure)>(original);
        return reinterpret_cast<FARPROC>(&configure_bridge);
    }
    if (std::strcmp(name, "ed_fm_suspension_feedback") == 0) {
        native_suspension = reinterpret_cast<decltype(native_suspension)>(original);
        return reinterpret_cast<FARPROC>(&suspension_feedback);
    }
    if (std::strcmp(name, "ed_fm_get_param") == 0) {
        native_parameter = reinterpret_cast<decltype(native_parameter)>(original);
        return reinterpret_cast<FARPROC>(&parameter);
    }
    if (std::strcmp(name, "ed_fm_get_internal_fuel") == 0) native_fuel = reinterpret_cast<decltype(native_fuel)>(original);
    if (std::strcmp(name, "ed_fm_simulate") == 0) {
        native_simulate = reinterpret_cast<decltype(native_simulate)>(original);
        return reinterpret_cast<FARPROC>(&simulate_native_engines);
    }
    if (std::strcmp(name, "ed_fm_set_draw_args_v2") == 0) {
        native_draw = reinterpret_cast<decltype(native_draw)>(original);
        return reinterpret_cast<FARPROC>(&draw_native);
    }
#define INPUT(n) if (std::strcmp(name, #n) == 0) { \
    Both<&own_##n>::native = reinterpret_cast<decltype(&own_##n)>(original); \
    return reinterpret_cast<FARPROC>(&Both<&own_##n>::invoke); }
    INPUT(ed_fm_set_atmosphere)
    INPUT(ed_fm_set_surface)
    INPUT(ed_fm_set_current_mass_state)
    INPUT(ed_fm_set_current_state)
    INPUT(ed_fm_set_current_state_body_axis)
    INPUT(ed_fm_set_command)
    INPUT(ed_fm_set_internal_fuel)
    INPUT(ed_fm_refueling_add_fuel)
    INPUT(ed_fm_cold_start)
    INPUT(ed_fm_hot_start)
    INPUT(ed_fm_hot_start_in_air)
    INPUT(ed_fm_release)
#undef INPUT
#define OUTPUT(n) if (std::strcmp(name, #n) == 0) return reinterpret_cast<FARPROC>(&own_##n);
    OUTPUT(ed_fm_add_local_force)
    OUTPUT(ed_fm_add_local_moment)
#undef OUTPUT
    return original;
}
