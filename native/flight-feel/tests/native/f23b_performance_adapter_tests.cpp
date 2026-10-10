// SPDX-License-Identifier: GPL-3.0-or-later
// Exercises the exported callbacks using the real installed SDK when available.
#include "../../src/efm/performance/dcs_adapter/performance_adapter.cpp"
#include <cstdlib>
#include <iostream>
#include <type_traits>
#ifdef F23B_REAL_SDK
static_assert(std::is_same_v<decltype(&ed_fm_set_current_state_body_axis), PFN_CURRENT_STATE_BODY_AXIS>);
static_assert(std::is_same_v<decltype(&ed_fm_set_surface), PFN_SET_SURFACE>);
#endif
void require(bool ok, const char* message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
void body(double ay, double pitch=0, double roll=0, double pitch_rate=0) {
    ed_fm_set_current_state_body_axis(0,ay,0,200,0,0,0,0,0,0,0,0,0,0,pitch_rate,0,pitch,roll,0,0);
}
int main() {
    ed_fm_hot_start_in_air();
    ed_fm_set_internal_fuel(4900);
    body(0);
    ed_fm_set_command(2005,-1); ed_fm_set_command(2006,1);
    for (int i=0;i<2000;++i) ed_fm_simulate(.01);
    require(runtime.output.engines[0].thrust_n > 5*runtime.output.engines[1].thrust_n,"split thrust");
    require(runtime.output.engines[0].fuel_flow_kg_s > runtime.output.engines[1].fuel_flow_kg_s,"split fuel flow");
    require(runtime.output.engines[0].afterburner_fraction > .9 && runtime.output.engines[1].afterburner_fraction == 0,"split burner");
    ed_fm_set_command(2004,0);
    require(runtime.input.throttle_left == .5 && runtime.input.throttle_right == .5,"combined axis");
    body(-5*kGravity); require(std::abs(runtime.body.normal_load_g+4)<1e-9,"negative g");
    body(-6*kGravity); runtime.input.pitch=-1; ed_fm_simulate(.01);
    require(runtime.output.controls.g_limiter_active,"negative-g limiter reachable from SDK callback");
    body(0,0,3.141592653589793); require(std::abs(runtime.body.normal_load_g+1)<1e-9,"inverted g");
    body(0,0,3.141592653589793/2); require(std::abs(runtime.body.normal_load_g)<1e-9,"bank gravity projection");
    body(-kGravity); require(std::abs(runtime.body.normal_load_g)<1e-9,"free fall");
    ed_fm_set_atmosphere(1010,288,340,1.2,101325,0,0,0);
    ed_fm_set_surface(1000,1000,0,0,1,0);
    require(runtime.body.altitude_agl_m == 10,"absolute terrain elevation");
    ed_fm_set_surface(1000,1005,0,0,1,0);
    require(runtime.body.altitude_agl_m == 5,"object elevation");
    runtime.input.gear_down=true; ed_fm_simulate(.01);
    require(!runtime.body.weight_on_wheels,"low approach is airborne");
    ed_fm_suspension_info contact{}; contact.struct_compression=.1;
    ed_fm_suspension_feedback(0,&contact); ed_fm_simulate(.01);
    require(runtime.body.weight_on_wheels,"suspension contact");
    contact.struct_compression=0; ed_fm_suspension_feedback(0,&contact); ed_fm_simulate(.01);
    require(!runtime.body.weight_on_wheels,"contact release");
    double trim[2]{};
    for(int run=0;run<2;++run) {
        ed_fm_hot_start_in_air(); ed_fm_set_internal_fuel(4900);
        const double dt=run==0?.005:.02;
        for(int i=0;i<(run==0?200:50);++i) { body(0,0,0,.00001); ed_fm_simulate(dt); }
        trim[run]=runtime.output.controls.pitch_command;
    }
    require(std::abs(trim[0]-trim[1])<1e-8,"unsaturated autotrim timestep invariance");
    std::cout << "Adapter callback regression checks passed\n";
}
