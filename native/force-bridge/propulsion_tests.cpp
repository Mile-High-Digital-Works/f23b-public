// SPDX-License-Identifier: GPL-3.0-or-later
#include "propulsion.hpp"
#include <cstdlib>
#include <limits>
using namespace f23b;
void near(double actual, double expected) { if(!std::isfinite(actual) || std::abs(actual-expected)>1e-7) std::abort(); }
int main() {
    performance::MassState mass;
    mass.center_of_mass_m = {-.65, -.03348, 0};
    performance::Output o;
    bridge::add_native_engine(o,mass,0,10000,.1,.9);
    bridge::add_native_engine(o,mass,1,10000,.1,.9);
    near(o.force_n.x,40000);
    near(o.moment_n_m.y,0); // symmetric thrust cannot add yaw at symmetric CG
    near(o.moment_n_m.z,(-.03348+.320670754)*40000);
    near(o.fuel_flow_kg_s,.2); // native fuel flow is counted once, never scaled
    o={};
    bridge::add_native_engine(o,mass,0,4000,0,.9); // native windmilling residual
    bridge::add_native_engine(o,mass,1,10000,.1,.9);
    near(o.force_n.x,20000);
    near(o.moment_n_m.y,1.311020017*20000);
    near(o.engines[0].thrust_n,0);
    o={};
    bridge::add_native_engine(o,mass,0,std::numeric_limits<double>::quiet_NaN(),.1,.9);
    bridge::add_native_engine(o,mass,1,10000,std::numeric_limits<double>::infinity(),.9);
    near(o.force_n.x,0);

    // Reproduce native readings from the previously rolling hot-start run.
    // No braking, contact state, or input-axis estimate participates in gain.
    for (double rpm : {0.0, .212347, .268791, .30}) {
        o = {};
        bridge::add_native_engine(o,mass,0,1755.0,.0916406,rpm);
        bridge::add_native_engine(o,mass,1,1755.0,.0916406,rpm);
        near(o.force_n.x,3510.0);
        near(o.fuel_flow_kg_s,.1832812);
        near(o.moment_n_m.y,0);
    }
    // MIL/AB retain the full accepted gain, even at low absolute thrust.
    for (double rpm : {.70, .886287, 1.0, 1.038962}) {
        for (double thrust : {1000.0, 10000.0, 107562.0}) {
            o = {};
            bridge::add_native_engine(o,mass,0,thrust,.1,rpm);
            near(o.force_n.x,2*thrust);
        }
    }
    double previous_gain=1;
    for (int i=0;i<=12000;++i) {
        const double rpm=i/10000.0;
        const double gain=bridge::native_engine_gain(rpm);
        if (gain<previous_gain || gain<1 || gain>2 || gain-previous_gain>.000376) std::abort();
        previous_gain=gain;
    }
    // Zero-slope endpoints prevent a new discontinuity on spool up/down.
    constexpr double h=1e-6;
    for (double boundary : {.30,.70}) {
        const double slope=(bridge::native_engine_gain(boundary+h)-bridge::native_engine_gain(boundary-h))/(2*h);
        if (std::abs(slope)>1e-4) std::abort();
    }
    // Independent engine state: one idle, one powered; native cutoff still wins.
    o={};
    bridge::add_native_engine(o,mass,0,1755,.09,.212347);
    bridge::add_native_engine(o,mass,1,10000,.2,.9);
    near(o.force_n.x,21755);
    near(o.moment_n_m.y,1.311020017*(20000-1755));
    o={};
    bridge::add_native_engine(o,mass,0,10000,0,.9);
    near(o.force_n.x,0);
    near(bridge::native_engine_gain(std::numeric_limits<double>::quiet_NaN()),1);
    near(bridge::native_engine_gain(std::numeric_limits<double>::infinity()),1);
    o={};
    bridge::add_native_engine(o,mass,0,-10000,.1,.9);
    near(o.force_n.x,0);

    // Compare the actual force model with the previous aerodynamic settings.
    // Increased power must still have zero pitching moment on the tested CG.
    mass.center_of_mass_m.y = -.320670754;
    o = {};
    bridge::add_native_engine(o,mass,0,10000,.1,.9);
    bridge::add_native_engine(o,mass,1,10000,.1,.9);
    near(o.moment_n_m.z,0);
    performance::Atmosphere air;
    air.altitude_m = 5000;
    performance::PilotInput input;
    input.engine_left_running = input.engine_right_running = false;
    auto sample = [&](double mach, bool tuned, double alpha, bool gear, bool brake, bool flaps) {
        performance::Model model(tuned ? bridge::flight_options : performance::Options{});
        model.reset(); // the tune must survive the normal start/reset lifecycle
        performance::BodyState body;
        body.velocity_body_m_s.x = mach * air.speed_of_sound_m_s;
        body.angle_of_attack_rad = alpha;
        input.gear_down = gear;
        input.speedbrake = brake;
        input.landing_flaps = flaps;
        return model.step(.01, air, mass, body, input);
    };
    for (double mach : {.2, .8, .9, 1.15, 1.4, 2.0, 2.4}) {
        const auto before = sample(mach,false,0,false,false,false);
        const auto after = sample(mach,true,0,false,false,false);
        const double ratio = mach <= .9 ? 1.0 : mach == 1.15 ? .96 : .92;
        near(after.drag_coefficient, before.drag_coefficient * ratio);
        near(after.force_n.x, before.force_n.x * ratio);
        near(after.lift_coefficient, before.lift_coefficient);
        near(after.controls.pitch_command, before.controls.pitch_command);
        near(after.controls.roll_rate_limit_rad_s, before.controls.roll_rate_limit_rad_s);
        // Gear, speedbrake, flap and AOA drag retain their full increments.
        const auto configured_before = sample(mach,false,.2,true,true,true);
        const auto configured_after = sample(mach,true,.2,true,true,true);
        near(configured_after.drag_coefficient - after.drag_coefficient,
             configured_before.drag_coefficient - before.drag_coefficient);
    }
}
