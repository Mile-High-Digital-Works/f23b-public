// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <filesystem>
#include <fstream>
#include <iomanip>

// This opt-in recorder reads the SDK state and calculated aerodynamic output.
// It does not feed a value back into the flight model.
inline void record_fit_sample(double time, double dt, const perf::Output& aero) {
    wchar_t path[2048]{};
    const auto length = GetEnvironmentVariableW(L"F23B_FORCE_BRIDGE_FIT", path, 2048);
    if (!length || length >= 2048) return;
    const auto filename = std::filesystem::path(path);
    const bool header = !std::filesystem::exists(filename);
    std::ofstream out(filename, std::ios::app);
    if (!out) return;
    if (header) out << "time,dt,rho,sound,alt,mass,cgx,cgy,cgz,ix,iy,iz,vx,vy,vz,wx,wy,wz,p,r,q,alpha,beta,bank,ng,agl,wow,pitch,roll,yaw,analog_pitch,analog_roll,analog_yaw,gear,brake,flaps,wheel_brake,fx,fy,fz,mx,my,mz,cmd_pitch,cmd_roll,cmd_yaw,elevon_l,elevon_r,world_x,world_y,world_z,world_vx,world_vy,world_vz\n";
    const auto& a = runtime.atmosphere;
    const auto& m = runtime.mass;
    const auto& b = runtime.body;
    const auto& i = runtime.input;
    const auto vector = [&](const perf::Vec3& v) { out << ',' << v.x << ',' << v.y << ',' << v.z; };
    out << std::setprecision(17) << time << ',' << dt << ',' << a.density_kg_m3 << ',' << a.speed_of_sound_m_s << ',' << a.altitude_m << ',' << m.mass_kg;
    vector(m.center_of_mass_m); vector(m.inertia_kg_m2);
    vector(b.velocity_body_m_s); vector(b.wind_body_m_s); vector(b.angular_rate_rad_s);
    out << ',' << b.angle_of_attack_rad << ',' << b.sideslip_rad << ',' << b.roll_rad << ',' << b.normal_load_g << ',' << b.altitude_agl_m << ',' << b.weight_on_wheels
        << ',' << i.pitch << ',' << i.roll << ',' << i.yaw << ',' << i.analog_pitch << ',' << i.analog_roll << ',' << i.analog_yaw
        << ',' << i.gear_down << ',' << i.speedbrake << ',' << i.landing_flaps << ',' << i.wheel_brake;
    vector(aero.force_n); vector(aero.moment_n_m);
    out << ',' << aero.controls.pitch_command << ',' << aero.controls.roll_command << ',' << aero.controls.yaw_command << ',' << aero.controls.left_elevon_rad << ',' << aero.controls.right_elevon_rad;
    vector(runtime.world_position); vector(runtime.world_velocity);
    out << '\n';
}
