// SPDX-License-Identifier: GPL-3.0-or-later
// Project SDK adapter. Native callbacks retain engines, fuel, gear and systems.
#pragma once
#include <FM/wHumanCustomPhysicsAPI.h>
#include "../flight-core/bridge_model.hpp"
#include "../flight-core/bridge_context.hpp"
#include <algorithm>
#include <array>
#include <cmath>
namespace perf=f23b::performance;
namespace {
struct RuntimeState {
    perf::Model model;
    perf::Atmosphere atmosphere;
    perf::MassState mass;
    perf::BodyState body;
    perf::PilotInput input;
    perf::Output output;
    perf::Vec3 world_position{},world_velocity{};
    std::array<double,3> suspension_compression{};
    double internal_fuel_kg{},keyboard_throttle_rate{};
};
RuntimeState runtime;
double finite_or(double value,double fallback=0) {return std::isfinite(value)?value:fallback;}
void reset_adapter(bool ground) {
    runtime=RuntimeState{};
    runtime.input.gear_down=ground;
    runtime.body.weight_on_wheels=ground;
    runtime.suspension_compression.fill(ground?.1:0.);
    runtime.input.throttle_left=runtime.input.throttle_right=ground?0:.775;
    f23b::flight_core::sdk_pitch_rad=0;
}
template<class Write> void write_control_arguments(std::size_t count,Write write) {
    const auto& c=runtime.output.controls;
    // Project surface travel: full command is 0.4 rad in bridge_model.cpp.
    // Channels are the existing exterior model's animation arguments.
    const std::array<double,10> values={c.flap,c.flap,c.left_aileron,c.right_aileron,
        c.left_elevon_rad/.4,c.right_elevon_rad/.4,c.left_rudder,c.right_rudder,
        c.leading_edge_flap,c.leading_edge_flap};
    for(std::size_t i=0;i<values.size();++i)
        if(9+i<count)write(9+i,static_cast<float>(std::clamp(finite_or(values[i]),-1.,1.)));
}
}
void own_ed_fm_set_atmosphere(double altitude,double,double sound,double density,double,double,double,double) {
    runtime.atmosphere={std::max(0.,finite_or(density,1.225)),std::max(1.,finite_or(sound,340.294)),finite_or(altitude)};
}
void own_ed_fm_set_surface(double height,double objects,unsigned,double,double,double) {
    runtime.body.altitude_agl_m=runtime.atmosphere.altitude_m-std::max(finite_or(height),finite_or(objects));
}
void own_ed_fm_set_current_mass_state(double mass,double x,double y,double z,double ix,double iy,double iz) {
    if(std::isfinite(mass)&&mass>0)runtime.mass.mass_kg=mass;
    runtime.mass.center_of_mass_m={finite_or(x),finite_or(y),finite_or(z)};
    if(std::isfinite(ix)&&ix>0)runtime.mass.inertia_kg_m2.x=ix;
    if(std::isfinite(iy)&&iy>0)runtime.mass.inertia_kg_m2.y=iy;
    if(std::isfinite(iz)&&iz>0)runtime.mass.inertia_kg_m2.z=iz;
}
void own_ed_fm_set_current_state(double,double,double,double vx,double vy,double vz,double px,double py,double pz,
    double,double,double,double,double,double,double,double,double,double) {
    runtime.world_velocity={vx,vy,vz};runtime.world_position={px,py,pz};
}
void own_ed_fm_set_current_state_body_axis(double,double ay,double,double vx,double vy,double vz,
    double wx,double wy,double wz,double,double,double,double p,double r,double q,double,double pitch,double bank,double alpha,double beta) {
    auto& b=runtime.body;
    b.velocity_body_m_s={vx,vy,vz};b.wind_body_m_s={wx,wy,wz};b.angular_rate_rad_s={p,r,q};
    b.angle_of_attack_rad=finite_or(alpha);b.sideslip_rad=finite_or(beta);b.roll_rad=finite_or(bank);
    f23b::flight_core::sdk_pitch_rad=finite_or(pitch);
    b.normal_load_g=finite_or(ay)/9.80665+std::cos(finite_or(pitch))*std::cos(b.roll_rad);
}
void own_ed_fm_set_command(int command,float value) {
    if(!std::isfinite(value))return;
    auto& i=runtime.input;
    // SDK axis contract, confirmed by the recorded card and bridge unit test.
    double* axes[]={&i.pitch,&i.roll,&i.yaw};
    bool* analog[]={&i.analog_pitch,&i.analog_roll,&i.analog_yaw};
    if(command>=2001&&command<=2003) {
        const int a=command-2001;
        *axes[a]=std::clamp(double(value)*(a==2?-1:1),-1.,1.);*analog[a]=true;return;
    }
    if(command>=2004&&command<=2006) {
        const double throttle=std::clamp((1.-value)*.5,0.,1.);
        if(command!=2006)i.throttle_left=throttle;
        if(command!=2005)i.throttle_right=throttle;
        runtime.keyboard_throttle_rate=0;return;
    }
    // Twelve standard keyboard start/stop commands: two directions per axis.
    if(command>=193&&command<=204) {
        const int slot=command-193,axis=slot/4;
        *axes[axis]=slot%2?0.:(slot%4==0?-1.:1.)*(axis==2?-1.:1.);
        *analog[axis]=false;return;
    }
    if(command==1032||command==1033||command==1034) {
        runtime.keyboard_throttle_rate=command==1032?1.:(command==1033?-1.:0.);return;
    }
    struct Switch {int toggle,on,off;bool* destination;};
    const Switch switches[]={{68,431,430,&i.gear_down},{73,147,148,&i.speedbrake},{72,146,145,&i.landing_flaps},{-1,74,75,&i.wheel_brake}};
    for(const auto& s:switches) {
        if(command==s.toggle)*s.destination=!*s.destination;
        else if(command==s.on)*s.destination=true;
        else if(command==s.off)*s.destination=false;
    }
}
void own_ed_fm_simulate(double dt) {
    if(!std::isfinite(dt)||dt<0){runtime.output={};return;}
    const int steps=std::max(1,static_cast<int>(std::ceil(std::min(dt,1.)/.02)));
    const double h=std::min(dt,1.)/steps;
    runtime.body.weight_on_wheels=std::any_of(runtime.suspension_compression.begin(),runtime.suspension_compression.end(),[](double c){return c>0;});
    for(int k=0;k<steps;++k) {
        if(runtime.keyboard_throttle_rate!=0) {
            const double t=std::clamp(.5*(runtime.input.throttle_left+runtime.input.throttle_right)+.2*h*runtime.keyboard_throttle_rate,0.,1.);
            runtime.input.throttle_left=runtime.input.throttle_right=t;
        }
        runtime.output=runtime.model.step(h,runtime.atmosphere,runtime.mass,runtime.body,runtime.input);
    }
}
void own_ed_fm_add_local_force(double& x,double& y,double& z,double& px,double& py,double& pz) {
    const auto& f=runtime.output.force_n;const auto& c=runtime.mass.center_of_mass_m;
    x=f.x;y=f.y;z=f.z;px=c.x;py=c.y;pz=c.z;
}
void own_ed_fm_add_local_moment(double& x,double& y,double& z) {
    const auto& m=runtime.output.moment_n_m;x=m.x;y=m.y;z=m.z;
}
void own_ed_fm_suspension_feedback(int index,const ed_fm_suspension_info* info) {
    if(info&&index>=0&&index<3&&std::isfinite(info->struct_compression))runtime.suspension_compression[index]=info->struct_compression;
}
void own_ed_fm_set_internal_fuel(double fuel) {runtime.internal_fuel_kg=std::max(0.,finite_or(fuel));}
void own_ed_fm_refueling_add_fuel(double fuel) {runtime.internal_fuel_kg=std::max(0.,runtime.internal_fuel_kg+finite_or(fuel));}
double own_ed_fm_get_internal_fuel() {return runtime.internal_fuel_kg;}
double own_ed_fm_get_shake_amplitude() {return runtime.output.shake_amplitude;}
void own_ed_fm_cold_start() {reset_adapter(true);}
void own_ed_fm_hot_start() {reset_adapter(true);}
void own_ed_fm_hot_start_in_air() {reset_adapter(false);}
void own_ed_fm_release() {runtime=RuntimeState{};}
void own_ed_fm_configure(const char*) {}
// Type anchors only. The selector forwards these calls to native Hornet code.
double own_ed_fm_get_param(unsigned) {return 0;}
void own_ed_fm_set_draw_args_v2(float*,std::size_t) {}
