// SPDX-License-Identifier: GPL-3.0-or-later
#include "f23b/performance/performance_model.hpp"
#include "f23b/performance/grinnelli_v21_reference.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>

namespace perf = f23b::performance;
namespace ref = f23b::performance::grinnelli_v21;

namespace {
int failures = 0;

void check(bool condition, std::string_view message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void check_near(double actual, double expected, double tolerance, std::string_view message) {
    if (!std::isfinite(actual) || std::abs(actual - expected) > tolerance) {
        ++failures;
        std::cerr << "FAIL: " << message << " actual=" << actual
                  << " expected=" << expected << " tol=" << tolerance << '\n';
    }
}

perf::BodyState body_for_mach(double mach, double alpha_deg = 0.0,
    double beta_deg = 0.0, double density = 1.225) {
    perf::BodyState body{};
    body.velocity_body_m_s.x = mach * 340.294;
    body.angle_of_attack_rad = alpha_deg * ref::deg_to_rad;
    body.sideslip_rad = beta_deg * ref::deg_to_rad;
    body.altitude_agl_m = 5000.0;
    body.normal_load_g = 1.0;
    (void)density;
    return body;
}

perf::Output settle_engine(perf::Model& model, double dt, double seconds,
    double throttle, double mach = 0.8, double density = 1.225) {
    perf::Atmosphere atmosphere{density, 340.294, 0.0};
    perf::MassState mass{};
    perf::BodyState body = body_for_mach(mach);
    perf::PilotInput input{};
    input.throttle_left = input.throttle_right = throttle;
    perf::Output out{};
    const int steps = static_cast<int>(std::ceil(seconds / dt));
    for (int i = 0; i < steps; ++i) {
        out = model.step(dt, atmosphere, mass, body, input);
    }
    return out;
}

void test_interpolation_and_shaping() {
    check_near(perf::interpolate(ref::mach.data(), ref::zero_lift_drag.data(),
        ref::mach.size(), 0.1), 0.01325, 1.0e-9, "linear interpolation");
    check_near(perf::shaped_axis(0.2), 0.04, 1.0e-12, "soft-center positive axis");
    check_near(perf::shaped_axis(-0.5), -0.25, 1.0e-12, "soft-center negative axis");
}

void test_engine_schedule_and_ab() {
    perf::Model model;
    const auto out = settle_engine(model, 0.01, 8.0, 1.0, 0.8);
    check(out.engines[0].combustion && out.engines[1].combustion,
        "both hot-air engines combust");
    check(out.engines[0].afterburner_fraction > 0.95,
        "afterburner reaches full continuous stage");
    check(out.engines[0].thrust_n > 150000.0,
        "per-engine normalized AB thrust remains Grinnelli-class");
    check(out.engines[0].spool_fraction > 0.95,
        "spool reaches full visual state");
    check(out.fuel_flow_kg_s > 0.0, "fuel flow is positive with combustion");
}

void test_mass_normalization() {
    perf::Atmosphere atmosphere{};
    perf::BodyState body = body_for_mach(0.8, 0.0, 0.0);
    perf::PilotInput input{};
    input.throttle_left = input.throttle_right = 1.0;

    perf::MassState light{};
    light.mass_kg = 14000.0;
    perf::MassState heavy = light;
    heavy.mass_kg = 28000.0;

    perf::Model a;
    perf::Model b;
    perf::Output oa{}, ob{};
    for (int i = 0; i < 800; ++i) {
        oa = a.step(0.01, atmosphere, light, body, input);
        ob = b.step(0.01, atmosphere, heavy, body, input);
    }
    check_near(oa.force_n.x / light.mass_kg, 2.0 * ob.force_n.x / heavy.mass_kg,
        1.0e-8, "doubling mass halves acceleration at fixed engine/aero force");
}

void test_dt_independence_engine() {
    perf::Model fine;
    perf::Model coarse;
    const auto a = settle_engine(fine, 0.005, 3.0, 1.0, 0.6);
    const auto b = settle_engine(coarse, 0.020, 3.0, 1.0, 0.6);
    check_near(a.engines[0].spool_fraction, b.engines[0].spool_fraction,
        1.0e-9, "engine spool is integration-step independent");
    check_near(a.engines[0].afterburner_fraction, b.engines[0].afterburner_fraction,
        1.0e-9, "AB staging is integration-step independent");
}

void test_pitch_limiters() {
    perf::Atmosphere atmosphere{};
    perf::MassState mass{};
    perf::PilotInput input{};
    input.pitch = 1.0;
    input.throttle_left = input.throttle_right = 0.6;

    perf::Model alpha_model;
    auto alpha_body = body_for_mach(0.4, 56.0, 0.0);
    const auto alpha_out = alpha_model.step(0.02, atmosphere, mass, alpha_body, input);
    check(alpha_out.controls.alpha_limiter_active,
        "positive-alpha protection activates at scheduled limit");
    check(alpha_out.controls.pitch_command < 0.10,
        "positive-alpha limiter smoothly removes further nose-up demand");

    perf::Model g_model;
    auto g_body = body_for_mach(0.7, 5.0, 0.0);
    g_body.normal_load_g = 12.0;
    const auto g_out = g_model.step(0.02, atmosphere, mass, g_body, input);
    check(g_out.controls.g_limiter_active, "11-g limiter activates above reference limit");
    check(g_out.controls.pitch_command <= 0.80,
        "g limiter reduces nose-up command rather than hard-clamping state");
}

void test_roll_stop_and_yaw_coordination() {
    perf::Atmosphere atmosphere{};
    perf::MassState mass{};
    perf::PilotInput neutral{};
    neutral.throttle_left = neutral.throttle_right = 0.6;

    perf::Model roll_model;
    auto rolling = body_for_mach(0.8, 5.0, 0.0);
    rolling.angular_rate_rad_s.x = 1.0;
    const auto roll_out = roll_model.step(0.02, atmosphere, mass, rolling, neutral);
    check(roll_out.controls.roll_command < 0.0,
        "neutral stick commands roll-rate damping");
    check(roll_out.moment_n_m.x < 0.0,
        "roll damping moment opposes positive roll rate");

    perf::Model yaw_model;
    auto slipped = body_for_mach(0.8, 5.0, 2.0);
    const auto yaw_out = yaw_model.step(0.02, atmosphere, mass, slipped, neutral);
    check(std::abs(yaw_out.controls.yaw_command) > 1.0e-6,
        "neutral pedals generate automatic sideslip correction");
}

void test_finite_grid() {
    perf::Atmosphere atmosphere{};
    perf::MassState mass{};
    for (double mach = 0.0; mach <= 2.4 + 1.0e-9; mach += 0.2) {
        for (double alpha : {-50.0, -20.0, 0.0, 20.0, 50.0}) {
            for (double beta : {-20.0, 0.0, 20.0}) {
                perf::Model model;
                perf::PilotInput input{};
                input.pitch = 0.4;
                input.roll = -0.3;
                input.yaw = 0.2;
                input.throttle_left = input.throttle_right = 1.0;
                const auto out = model.step(0.02, atmosphere, mass,
                    body_for_mach(mach, alpha, beta), input);
                const double values[] = {
                    out.force_n.x, out.force_n.y, out.force_n.z,
                    out.moment_n_m.x, out.moment_n_m.y, out.moment_n_m.z,
                    out.lift_coefficient, out.drag_coefficient,
                    out.controls.pitch_command, out.controls.roll_command,
                    out.controls.yaw_command,
                };
                for (double value : values) {
                    check(std::isfinite(value), "entire envelope grid remains finite");
                }
            }
        }
    }
}


void test_dt_independence_control_commands() {
    perf::Atmosphere atmosphere{};
    perf::MassState mass{};
    auto body = body_for_mach(0.8, 5.0, 0.0);
    perf::PilotInput input{};
    input.pitch = 0.65;
    input.roll = -0.55;
    input.yaw = 0.20;
    input.analog_pitch = false;
    input.analog_roll = false;
    input.analog_yaw = false;
    input.throttle_left = input.throttle_right = 0.7;

    perf::Model fine;
    perf::Model coarse;
    perf::Output a{}, b{};
    for (int i = 0; i < 200; ++i) {
        a = fine.step(0.005, atmosphere, mass, body, input);
    }
    for (int i = 0; i < 50; ++i) {
        b = coarse.step(0.020, atmosphere, mass, body, input);
    }
    check_near(a.controls.pitch_command, b.controls.pitch_command, 1.0e-9,
        "discrete pitch shaping is integration-step independent");
    check(a.controls.roll_command * b.controls.roll_command > 0.0
        && std::abs(b.controls.roll_command) <= std::abs(a.controls.roll_command),
        "coarser updates retain roll direction with conservative authority");
    check(std::isfinite(a.controls.yaw_command) && std::isfinite(b.controls.yaw_command)
        && std::abs(a.controls.yaw_command - b.controls.yaw_command) < 0.01,
        "adverse-yaw coordination remains bounded across update rates");
}

void test_alpha_recovery_authority() {
    perf::Atmosphere atmosphere{};
    perf::MassState mass{};
    auto body = body_for_mach(0.4, 65.0, 0.0);
    perf::PilotInput nose_up{};
    nose_up.pitch = 1.0;
    perf::PilotInput nose_down{};
    nose_down.pitch = -1.0;
    perf::Model up_model;
    perf::Model down_model;
    const auto up = up_model.step(0.02, atmosphere, mass, body, nose_up);
    const auto down = down_model.step(0.02, atmosphere, mass, body, nose_down);
    check(up.controls.alpha_limiter_active,
        "over-limit nose-up demand is limited");
    check(down.controls.pitch_command < -0.5,
        "over-limit nose-down recovery retains strong authority");
}

void test_angular_acceleration_normalization() {
    perf::Atmosphere atmosphere{};
    perf::PilotInput input{};
    input.roll = 0.6;
    input.throttle_left = input.throttle_right = 0.5;
    auto body = body_for_mach(0.7, 4.0, 0.0);

    perf::MassState low_inertia{};
    low_inertia.inertia_kg_m2.x = 30'000.0;
    perf::MassState high_inertia = low_inertia;
    high_inertia.inertia_kg_m2.x = 90'000.0;

    perf::Model a;
    perf::Model b;
    const auto oa = a.step(0.02, atmosphere, low_inertia, body, input);
    const auto ob = b.step(0.02, atmosphere, high_inertia, body, input);
    check(oa.moment_n_m.x > 0.0 && ob.moment_n_m.x > oa.moment_n_m.x
        && oa.moment_n_m.x / low_inertia.inertia_kg_m2.x
            > ob.moment_n_m.x / high_inertia.inertia_kg_m2.x,
        "inertia schedule compensates torque without reversing acceleration ordering");
}

void test_engine_independence_and_no_hidden_tvc() {
    perf::Atmosphere atmosphere{};
    perf::MassState mass{};
    perf::BodyState body = body_for_mach(0.3, 0.0, 0.0);
    perf::PilotInput input{};
    input.throttle_left = input.throttle_right = 1.0;
    input.engine_right_running = false;
    perf::Model model;
    perf::Output out{};
    for (int i = 0; i < 800; ++i) {
        out = model.step(0.01, atmosphere, mass, body, input);
    }
    check(out.engines[0].thrust_n > 0.0 && out.engines[1].thrust_n == 0.0,
        "per-engine state remains independent");
    // At alpha=0 the only vertical force is the small aerodynamic lift bias;
    // engine output does not inject a hidden pitch-vector force.
    const double expected_bias_lift = ref::zero_lift_bias * out.dynamic_pressure_pa
        * ref::wing_area_m2;
    check_near(out.force_n.y, expected_bias_lift, std::max(1.0, std::abs(expected_bias_lift) * 1.0e-8),
        "virtual thrust vectoring remains disabled");
}

} // namespace

int main() {
    test_interpolation_and_shaping();
    test_engine_schedule_and_ab();
    test_mass_normalization();
    test_dt_independence_engine();
    test_pitch_limiters();
    test_roll_stop_and_yaw_coordination();
    test_finite_grid();
    test_dt_independence_control_commands();
    test_alpha_recovery_authority();
    test_angular_acceleration_normalization();
    test_engine_independence_and_no_hidden_tvc();

    if (failures != 0) {
        std::cerr << failures << " performance-model test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All F-23B Grinnelli flight-feel offline tests passed\n";
    return EXIT_SUCCESS;
}
