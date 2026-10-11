// SPDX-License-Identifier: GPL-3.0-or-later
// Exercise the compiled SDK callback path and the actual replacement Model.
#include "../force-bridge/bridge.cpp"
#include "core.hpp"
#include "fitted_parameters.hpp"
#include <iostream>
#include <stdexcept>
namespace core=f23b::flight_core;
void check(bool ok,const char* name) { if(!ok)throw std::runtime_error(name); }
void native_body(double,double,double,double,double,double,double,double,double,
                 double,double,double,double,double,double,double,double,double,double,double) {}
void native_start() {}
int main() {
    static_assert(std::is_same_v<decltype(&own_ed_fm_set_current_state),PFN_CURRENT_STATE>);
    static_assert(std::is_same_v<decltype(&own_ed_fm_set_current_state_body_axis),PFN_CURRENT_STATE_BODY_AXIS>);
    static_assert(std::is_same_v<decltype(&own_ed_fm_set_current_mass_state),PFN_CURRENT_MASS_STATE>);
    static_assert(std::is_same_v<decltype(&own_ed_fm_suspension_feedback),PFN_FM_SUSPENSION_FEEDBACK>);
    using Callback=decltype(&own_ed_fm_set_current_state_body_axis);
    auto body=reinterpret_cast<Callback>(f23b_select_callback("ed_fm_set_current_state_body_axis",reinterpret_cast<FARPROC>(&native_body)));
    own_ed_fm_hot_start_in_air();
    own_ed_fm_set_atmosphere(4572,258,322,0.771,57000,0,0,0);
    own_ed_fm_set_current_mass_state(19300,-.65,-.320670754,0,31087,240989,168771);
    body(0,0,0,220,-12,2,0,0,0,0,0,0,.02,.01,.03,.5,.42,.15,.055,.009);
    check(core::sdk_pitch_rad==.42,"SDK pitch callback must reach the core");
    own_ed_fm_set_command(2001,.3f);own_ed_fm_set_command(2002,-.2f);own_ed_fm_set_command(2003,.4f);
    own_ed_fm_set_command(2005,-.55f);own_ed_fm_set_command(2006,-1.f);
    check(std::abs(runtime.input.throttle_left-.775)<1e-6&&runtime.input.throttle_right==1.,"recorded MIL and AB command mapping");
    std::cout<<"SDK command signs: pitch="<<runtime.input.pitch<<" roll="<<runtime.input.roll<<" yaw="<<runtime.input.yaw<<'\n';
    check(std::abs(runtime.input.pitch-.3)<1e-6&&std::abs(runtime.input.roll+.2)<1e-6&&std::abs(runtime.input.yaw+.4)<1e-6,"SDK command polarity");
    own_ed_fm_set_command(203,1);check(runtime.input.yaw==-1,"keyboard right pedal agrees with the SDK axis sign");
    own_ed_fm_set_command(204,1);check(runtime.input.yaw==0,"keyboard pedal release");
    own_ed_fm_set_command(2003,.4f);
    runtime.input.engine_left_running=runtime.input.engine_right_running=false;
    const auto& b=runtime.body;const auto& a=runtime.atmosphere;const auto& m=runtime.mass;const auto& u=runtime.input;
    core::State state{{b.velocity_body_m_s.x,b.velocity_body_m_s.y,b.velocity_body_m_s.z},
        {b.wind_body_m_s.x,b.wind_body_m_s.y,b.wind_body_m_s.z},
        {b.angular_rate_rad_s.x,b.angular_rate_rad_s.y,b.angular_rate_rad_s.z},
        {m.inertia_kg_m2.x,m.inertia_kg_m2.y,m.inertia_kg_m2.z},a.density_kg_m3,a.speed_of_sound_m_s,
        m.mass_kg,b.angle_of_attack_rad,b.sideslip_rad,core::sdk_pitch_rad,b.roll_rad,b.normal_load_g,b.weight_on_wheels};
    const auto expected=core::evaluate(fitted_parameters(),state,{u.pitch,u.roll,u.yaw,double(u.speedbrake),double(u.gear_down),double(u.landing_flaps)});
    const auto actual=runtime.model.step(.006,a,m,b,u);
    check(expected.valid,"candidate state must be valid");
    check(actual.force_n.x==expected.force.x&&actual.force_n.y==expected.force.y&&actual.force_n.z==expected.force.z,"bridge force matches the calibrated core");
    check(actual.moment_n_m.x==expected.moment.x&&actual.moment_n_m.y==expected.moment.y&&actual.moment_n_m.z==expected.moment.z,"bridge moment matches the calibrated core");
    check(actual.engines[0].thrust_n==0&&actual.engines[1].thrust_n==0&&actual.fuel_flow_kg_s==0,"no duplicate propulsion");
    auto with_thrust=actual;
    f23b::bridge::add_native_engine(with_thrust,m,0,10000,.1,.9);
    f23b::bridge::add_native_engine(with_thrust,m,1,10000,.1,.9);
    check(std::abs(with_thrust.force_n.x-actual.force_n.x-40000)<1e-7,"native thrust gain retained");
    check(std::abs(with_thrust.moment_n_m.z-actual.moment_n_m.z)<1e-7,"retained CG and thrust axis agree");
    perf::BodyState rest;rest.weight_on_wheels=true;rest.normal_load_g=1;
    perf::Model model;
    const auto stopped=model.step(.006,a,m,rest,u);
    check(stopped.force_n.x==0&&stopped.force_n.y==0&&stopped.force_n.z==0&&stopped.moment_n_m.x==0&&stopped.moment_n_m.y==0&&stopped.moment_n_m.z==0,"actual bridge model at rest");
    // Use the actual start callback, with an isolated profile. File changes
    // during a flight cannot change active parameters until the next start.
    const auto profile=std::filesystem::temp_directory_path()/("F23B-tuning-test-"+std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(profile/"Config");std::filesystem::create_directories(profile/"Logs");
    SetEnvironmentVariableW(L"F23B_SAVED_GAMES",profile.c_str());
    auto start=reinterpret_cast<void(*)()>(f23b_select_callback("ed_fm_hot_start_in_air",reinterpret_cast<FARPROC>(&native_start)));
    start();const auto baseline=fitted_parameters();
    check(std::memcmp(&core::runtime_parameters,&baseline,sizeof(baseline))==0,"start without tuning is byte-identical");
    std::ofstream(profile/"Config/F23B-tuning.json")<<R"({"tau_roll":1000,"unknown":9})";
    start();check(core::runtime_parameters.tau_roll==.4,"start applies clamped file");
    std::ofstream(profile/"Config/F23B-tuning.json")<<R"({"tau_roll":0.025})";
    runtime.model.step(.006,a,m,b,u);
    check(core::runtime_parameters.tau_roll==.4,"file is not reread in simulation");
    start();check(core::runtime_parameters.tau_roll==.025,"next aircraft start rereads file");
    std::filesystem::rename(profile/"Config/F23B-tuning.json",profile/"tested-tuning.json");
    start();check(std::memcmp(&core::runtime_parameters,&baseline,sizeof(baseline))==0,"absent file clears prior overrides");
    std::ifstream tuning_log(profile/"Logs/F23B-tuning.log");
    const std::string logged((std::istreambuf_iterator<char>(tuning_log)),std::istreambuf_iterator<char>());
    check(logged.find("ignored unknown key: unknown")!=std::string::npos&&logged.find("applied tau_roll=0.4")!=std::string::npos,"runtime override log");
    std::cout<<"INDEPENDENT BRIDGE PASS: SDK state/control mapping, exact force/moment, native propulsion, rest output\n";
}
