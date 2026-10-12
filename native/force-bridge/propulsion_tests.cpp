// SPDX-License-Identifier: GPL-3.0-or-later
#include "propulsion.hpp"
#include <cstdlib>
#include <limits>
using namespace f23b;
void near(double actual, double expected) { if(!std::isfinite(actual) || std::abs(actual-expected)>1e-7) std::abort(); }
int main() {
    const DryThrustSettings dry{true};
    for(double mach:{0.,1.,1.3,1.6,2.,2.5})for(double rpm:{0.,.212,.5,.9,1.1}) {
        near(bridge::development_engine_gain(rpm,mach,false,DryThrustSettings{false}),bridge::native_engine_gain(rpm));
        near(bridge::development_engine_gain(rpm,mach,true,dry),bridge::native_engine_gain(rpm));
        if(mach<=1.3)near(bridge::development_engine_gain(rpm,mach,false,dry),bridge::native_engine_gain(rpm));
    }
    near(bridge::development_engine_gain(.9,2.,false,dry),dry.end_gain);
    near(bridge::development_engine_gain(.212,2.,false,dry),1.);
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

    // The default dry curve changes only supersonic dry power.
    const DryThrustSettings defaults;
    near(bridge::development_engine_gain(.9,2.,false,defaults),defaults.end_gain);
    near(bridge::development_engine_gain(.9,2.,true,defaults),2.);
    near(bridge::development_engine_gain(.9,1.2221,false,defaults),2.);

    // Requested idle checks use the real force/moment/fuel output path.
    for(double mach:{0.,.9,1.2,1.25,1.3,1.6,2.,2.47})
        for(double throttle:{0.,.2,.5,.774,.775,.9,1.})
            for(double rpm:{.212,.5,.93,1.05}) {
                performance::Output before,after;
                const bool augmented=throttle>.775001;
                bridge::add_native_engine(before,mass,0,10000,.1,rpm,mach,augmented,defaults);
                bridge::add_native_engine(after,mass,0,10000,.1,rpm,mach,augmented,defaults,throttle);
                if(mach<=1.2||throttle>=.775) {
                    if(after.force_n.x!=before.force_n.x||after.moment_n_m.y!=before.moment_n_m.y
                        ||after.moment_n_m.z!=before.moment_n_m.z)std::abort();
                }
                near(after.fuel_flow_kg_s,before.fuel_flow_kg_s);
                near(bridge::supersonic_idle_factor(mach,throttle,DryThrustSettings{false}),1.);
            }
    performance::Output idle,military;
    bridge::add_native_engine(military,mass,0,10000,.1,.93,1.6,false,defaults,.775);
    bridge::add_native_engine(idle,mass,0,10000,.1,.93,1.6,false,defaults,0.);
    near(idle.force_n.x,bridge::supersonic_idle_fraction*military.force_n.x);
    double previous_thrust=-1.;
    for(int i=0;i<=1000;++i) {
        performance::Output sample;
        bridge::add_native_engine(sample,mass,0,10000,.1,.93,1.6,false,defaults,.775*i/1000.);
        if(sample.force_n.x<=previous_thrust)std::abort();
        previous_thrust=sample.force_n.x;
    }
}
