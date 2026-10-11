// SPDX-License-Identifier: GPL-3.0-or-later
// Original adapter for the project's retained force-bridge interface.
// Aerodynamics come only from core.hpp and the recorded fitted parameters.
#include "bridge_model.hpp"
#include "core.hpp"
#include "fitted_parameters.hpp"
#include "bridge_context.hpp"
#include "tuning_runtime.hpp"
namespace f23b::performance {
namespace core=f23b::flight_core;
namespace {
core::Vec3 vector(Vec3 v) { return {v.x,v.y,v.z}; }
Vec3 vector(core::Vec3 v) { return {v.x,v.y,v.z}; }
}
void Model::reset() noexcept {
    output_={};
    last_pitch_command_=last_roll_command_=last_yaw_command_=0;
    flap_position_=0;
    airborne_yaw_time_s_=.5;
    low_speed_recovery_=false;
}
Output Model::step(double dt,const Atmosphere& air,const MassState& mass,
                   const BodyState& body,const PilotInput& input) noexcept {
    const auto& parameters=core::runtime_parameters;
    output_={};
    if(!std::isfinite(dt)||dt<0)return output_;
    core::State s{vector(body.velocity_body_m_s),vector(body.wind_body_m_s),
        vector(body.angular_rate_rad_s),vector(mass.inertia_kg_m2),air.density_kg_m3,
        air.speed_of_sound_m_s,mass.mass_kg,body.angle_of_attack_rad,body.sideslip_rad,
        core::sdk_pitch_rad,body.roll_rad,body.normal_load_g,body.weight_on_wheels};
    low_speed_recovery_=core::recovery_switch(low_speed_recovery_,parameters,s);
    s.recovery_active=low_speed_recovery_;
    core::Input u{input.pitch,input.roll,input.yaw,double(input.speedbrake),
                  double(input.gear_down),double(input.landing_flaps)};
    const auto result=core::evaluate(parameters,s,u);
    if(!result.valid)return output_;
    output_.force_n=vector(result.force);output_.moment_n_m=vector(result.moment);
    // Preserve the existing project ground-yaw contract (ground_yaw_tests.cpp):
    // 5,000 N m s/rad damping, no beta steering aid, half-second lift-off blend.
    // This is bridge ground handling, not a fitted airborne coefficient.
    const double ground_yaw=-5000.*body.angular_rate_rad_s.y;
    if(body.weight_on_wheels)airborne_yaw_time_s_=0;
    const double blend=std::clamp(airborne_yaw_time_s_/.5,0.,1.);
    output_.moment_n_m.y=core::bounded(ground_yaw+(output_.moment_n_m.y-ground_yaw)*blend,parameters.moment_bound_nm);
    if(!body.weight_on_wheels)airborne_yaw_time_s_=std::min(.5,airborne_yaw_time_s_+dt);
    const auto velocity=s.velocity_body-s.wind_body;
    const double speed=std::sqrt(core::dot(velocity,velocity));
    output_.airspeed_m_s=speed;output_.mach=speed/s.sound;
    output_.indicated_airspeed_m_s=speed*std::sqrt(s.density/core::sea_level_density);
    output_.dynamic_pressure_pa=result.dynamic_pressure;
    const double scale=result.dynamic_pressure*parameters.area_m2;
    if(scale>0) {
        output_.lift_coefficient=result.lift/scale;output_.drag_coefficient=result.drag/scale;
        output_.side_force_coefficient=parameters.cy_beta*s.beta;
    }
    // Presentation only: project gain/rate choices, never force feedback.
    // Inputs are normalized SDK controls. The retained writer owns draw indices.
    last_pitch_command_=core::surface_position(last_pitch_command_,u.pitch,1,4,dt);
    last_roll_command_=core::surface_position(last_roll_command_,u.roll,1,4,dt);
    last_yaw_command_=core::surface_position(last_yaw_command_,u.pedal,1,4,dt);
    auto& c=output_.controls;
    c.pitch_command=last_pitch_command_;c.roll_command=last_roll_command_;c.yaw_command=last_yaw_command_;
    c.left_elevon_rad=.4*std::clamp(-c.pitch_command-c.roll_command,-1.,1.);
    c.right_elevon_rad=.4*std::clamp(-c.pitch_command+c.roll_command,-1.,1.);
    c.left_aileron=-c.roll_command;c.right_aileron=c.roll_command;
    c.left_rudder=c.right_rudder=c.yaw_command;
    flap_position_=core::surface_position(flap_position_,u.flap,1,.5,dt);
    c.flap=flap_position_;c.leading_edge_flap=flap_position_;
    c.angle_of_attack_limit_deg=core::sample(parameters.alpha_limit,output_.mach)*180/3.141592653589793;
    c.roll_rate_limit_rad_s=core::sample(parameters.roll_limit,output_.indicated_airspeed_m_s*core::knots_per_m_s);
    c.alpha_limiter_active=s.alpha>=core::sample(parameters.alpha_limit,output_.mach);
    c.g_limiter_active=s.normal_g>parameters.n_max||s.normal_g<parameters.n_min;
    return output_;
}
}
