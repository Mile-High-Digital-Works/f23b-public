// SPDX-License-Identifier: GPL-3.0-or-later
#include <f23b/performance/performance_model.hpp>
#include <cmath>
#include <cstdio>
#include <cstdlib>
using namespace f23b::performance;
void require(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
int main() {
    Atmosphere air;
    MassState mass;
    PilotInput input;
    input.engine_left_running = input.engine_right_running = false;
    BodyState body;
    body.weight_on_wheels = true;
    // At zero dynamic pressure beta must not create a turning force.
    for (double beta : {-1.0, -.1, .1, 1.0}) {
        Model model;
        body.sideslip_rad = beta;
        require(std::abs(model.step(.01, air, mass, body, input).moment_n_m.y) < 1e-8,
            "parked beta creates artificial yaw torque");
    }
    // Rate damping still opposes actual rotation, including on the ground.
    body.sideslip_rad = 0;
    body.angular_rate_rad_s.y = .1;
    Model damping;
    require(damping.step(.01, air, mass, body, input).moment_n_m.y < 0,
        "ground yaw damping lost");
    // Taxi turns keep 10 percent of the fixed 50 kNm s/rad rate term.
    Model taxi;
    require(std::abs(taxi.step(.01, air, mass, body, input).moment_n_m.y + 500) < 1,
        "taxi yaw damping not reduced to 10 percent");
    body.angular_rate_rad_s.y = 0;
    // A light crosswind retains small physical forces, not the old 5 kNm aid.
    body.wind_body_m_s = {-2, 0, -.2};
    body.sideslip_rad = .1;
    Model wind;
    auto w = wind.step(.01, air, mass, body, input);
    require(std::abs(w.moment_n_m.y) < 100, "crosswind ground torque excessive");
    require(std::abs(w.force_n.z) > 0, "physical crosswind force removed");
    // Compare otherwise identical airborne starts and a ground-to-air transition.
    body.wind_body_m_s = {};
    body.velocity_body_m_s.x = 100;
    Model takeoff, airborne;
    const auto ground = takeoff.step(.01, air, mass, body, input);
    body.weight_on_wheels = false;
    const auto full = airborne.step(.01, air, mass, body, input);
    auto first = takeoff.step(.01, air, mass, body, input);
    const double correction = full.moment_n_m.y - ground.moment_n_m.y;
    require(std::abs(correction) > 4000, "test must exercise original airborne correction");
    require(std::abs(first.moment_n_m.y - ground.moment_n_m.y) < std::abs(correction)*.02,
        "liftoff yaw step");
    for (int i=0; i<100; ++i) {
        auto a=takeoff.step(.01,air,mass,body,input);
        auto b=airborne.step(.01,air,mass,body,input);
        if (i>50) require(std::abs(a.moment_n_m.y-b.moment_n_m.y)<1e-6,
            "airborne handling does not recover");
    }
    body.weight_on_wheels = true;
    body.velocity_body_m_s = {};
    require(std::abs(takeoff.step(.01,air,mass,body,input).moment_n_m.y)<1e-8,
        "touchdown retains beta torque");
}
