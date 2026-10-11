// SPDX-License-Identifier: GPL-3.0-or-later
// Control experiment only. Link the installed build's existing core library.
// This interface wrapper supplies no old-model values to the new fit.
#pragma once
#include <f23b/performance/performance_model.hpp>
#include "../force-bridge/propulsion.hpp"
struct InstalledControl {
    f23b::performance::Model model{f23b::bridge::flight_options};
    double normal_g{1};
    bool initialized{};
    f23b::flight_core::Output last{};
    f23b::flight_core::Output step(const f23b::flight_core::State& s,
        const f23b::flight_core::Input& i,double altitude,double dt) {
        namespace p=f23b::performance;
        if(dt==0&&initialized) return last;
        p::Atmosphere atmosphere{s.density,s.sound,altitude};
        p::MassState mass{s.mass,{0,0,0},{s.inertia.x,s.inertia.y,s.inertia.z}};
        p::BodyState body;
        body.velocity_body_m_s={s.velocity_body.x,s.velocity_body.y,s.velocity_body.z};
        body.wind_body_m_s={s.wind_body.x,s.wind_body.y,s.wind_body.z};
        body.angular_rate_rad_s={s.angular_rate.x,s.angular_rate.y,s.angular_rate.z};
        body.angle_of_attack_rad=s.alpha;body.sideslip_rad=s.beta;
        body.roll_rad=s.bank;body.normal_load_g=normal_g;
        // Flown airborne segments only. The old test had no ground contact.
        body.altitude_agl_m=1000;body.weight_on_wheels=false;
        p::PilotInput pilot;
        pilot.pitch=i.pitch;pilot.roll=i.roll;pilot.yaw=-i.pedal;
        pilot.gear_down=i.gear>.5;pilot.speedbrake=i.brake>.5;pilot.landing_flaps=i.flap>.5;
        pilot.engine_left_running=pilot.engine_right_running=false;
        auto o=model.step(dt,atmosphere,mass,body,pilot);
        normal_g=o.force_n.y/(s.mass*f23b::flight_core::gravity);
        f23b::flight_core::Output result;
        result.force={o.force_n.x,o.force_n.y,o.force_n.z};
        result.moment={o.moment_n_m.x,o.moment_n_m.y,o.moment_n_m.z};
        result.dynamic_pressure=o.dynamic_pressure_pa;
        result.valid=std::isfinite(normal_g)&&std::isfinite(o.moment_n_m.x)
            &&std::isfinite(o.moment_n_m.y)&&std::isfinite(o.moment_n_m.z);
        initialized=true;last=result;return result;
    }
};
