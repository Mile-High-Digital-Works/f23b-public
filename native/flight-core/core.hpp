// SPDX-License-Identifier: MIT
// Original implementation of F23B_V15_FLIGHT_CORE_SPEC.md. No propulsion.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace f23b::flight_core {
constexpr double gravity = 9.80665;
constexpr double sea_level_density = 1.225;
constexpr double knots_per_m_s = 1.943844;
struct Vec3 { double x{}, y{}, z{}; };
inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
inline Vec3 operator*(Vec3 a, double b) { return {a.x*b,a.y*b,a.z*b}; }
inline double dot(Vec3 a, Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
inline bool finite(Vec3 a) { return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z); }
template<std::size_t N> struct Table { std::array<double,N> x{}, y{}; };
template<std::size_t N> inline double sample(const Table<N>& table,double value) {
    if(value<=table.x.front()) return table.y.front();
    for(std::size_t i=1;i<N;++i) if(value<=table.x[i]) {
        const double width=table.x[i]-table.x[i-1];
        if(!(width>0)) return 0;
        const double u=(value-table.x[i-1])/width;
        return table.y[i-1]+u*(table.y[i]-table.y[i-1]);
    }
    return table.y.back();
}
struct Parameters {
    double area_m2{};
    Table<12> cla, alpha0, cd0, induced_k, clmax, alpha_limit;
    Table<2> roll_limit;
    double dcd_brake{},dcd_gear{},dcd_flap{},flap_lift{},high_alpha_drag{};
    double n_max{},n_min{},wp{},kn{},pitch_damping{},tau_roll{};
    double k_beta{},k_r{},cy_beta{},tau_yaw{},pedal_rate{};
    double yaw_alpha_power_gain{},alpha_guard_gain{},alpha_guard_power_gain{},neutral_stick_width{};
    Vec3 control_power_per_pa{};
    double lift_break_fraction{},post_stall_decrease_per_rad{},alpha_feedback_s_inv{};
    double force_bound_n{},moment_bound_nm{};
    // Carefree handling (project choices, owner goal of October 11). Low-speed
    // recovery: below recovery_kias with the nose above recovery_pitch the law
    // commands a pushover toward recovery_path. Zero speed disables it.
    double recovery_kias{260},recovery_pitch_rad{.7853981633974483},recovery_path_rad{-.08726646259971647};
    // Pilot roll and pedal commands fall linearly from full at roll_alpha_start
    // to high_alpha_roll_floor at the alpha limit. A floor of one disables this.
    // Off by default: the offline corner sweep shows sideslip below 0.4 degrees
    // without it, so full roll authority stays until a DCS corner shows a need.
    double roll_alpha_start_rad{.3490658503988659},high_alpha_roll_floor{1.};
    double positive_load_limit{},negative_load_limit{-1};
};
struct State {
    Vec3 velocity_body{},wind_body{},angular_rate{},inertia{};
    double density{},sound{},mass{},alpha{},beta{},pitch{},bank{},normal_g{};
    bool weight_on_wheels{};
    bool recovery_active{};
};
struct Input { double pitch{},roll{},pedal{},brake{},gear{},flap{}; };
struct Output { Vec3 force{},moment{},wanted_acceleration{}; double lift{},drag{},load_command{},dynamic_pressure{}; bool valid{}; };
template<std::size_t N> inline bool valid_table(const Table<N>& table) {
    for(std::size_t i=0;i<N;++i) {
        if(!std::isfinite(table.x[i])||!std::isfinite(table.y[i])) return false;
        if(i>0&&!(table.x[i]>table.x[i-1])) return false;
    }
    return true;
}
inline bool valid_parameters(const Parameters& p) {
    if(!valid_table(p.cla)||!valid_table(p.alpha0)||!valid_table(p.cd0)||!valid_table(p.induced_k)
       ||!valid_table(p.clmax)||!valid_table(p.alpha_limit)||!valid_table(p.roll_limit)
       ||!finite(p.control_power_per_pa)) return false;
    for(double value:{p.area_m2,p.dcd_brake,p.dcd_gear,p.dcd_flap,p.flap_lift,p.n_max,p.n_min,
                      p.wp,p.kn,p.pitch_damping,p.tau_roll,p.k_beta,p.k_r,p.cy_beta,p.tau_yaw,p.pedal_rate,p.high_alpha_drag,
                      p.yaw_alpha_power_gain,p.alpha_guard_gain,p.alpha_guard_power_gain,p.neutral_stick_width,
                      p.lift_break_fraction,p.post_stall_decrease_per_rad,p.alpha_feedback_s_inv,
                      p.force_bound_n,p.moment_bound_nm,p.recovery_kias,p.recovery_pitch_rad,p.recovery_path_rad,
                      p.roll_alpha_start_rad,p.high_alpha_roll_floor,p.positive_load_limit,p.negative_load_limit}) if(!std::isfinite(value)) return false;
    if(p.high_alpha_roll_floor<0||p.high_alpha_roll_floor>1) return false;
    return p.area_m2>0&&p.force_bound_n>0&&p.moment_bound_nm>0&&p.tau_roll>0&&p.tau_yaw>0
        &&p.lift_break_fraction>0&&p.lift_break_fraction<1&&p.control_power_per_pa.x>=0
        &&p.control_power_per_pa.y>=0&&p.control_power_per_pa.z>=0&&p.high_alpha_drag>=0
        &&p.yaw_alpha_power_gain>=0&&p.alpha_guard_gain>=0&&p.alpha_guard_power_gain>=0&&p.neutral_stick_width>=0;
}
inline double bounded(double v,double limit) { return std::clamp(v,-limit,limit); }
inline double flight_path_angle(const State& s) {
    const auto v=s.velocity_body-s.wind_body;const double speed=std::sqrt(dot(v,v));
    if(speed<1e-6)return 0;
    return std::asin(std::clamp((v.x*std::sin(s.pitch)+v.y*std::cos(s.pitch)*std::cos(s.bank)
        -v.z*std::cos(s.pitch)*std::sin(s.bank))/speed,-1.,1.));
}
// Explicit controller state, owned by the caller. The aerodynamic evaluator
// remains a pure function. Hysteresis keeps a started recovery active until
// the flight path is nose-low, rather than releasing at the entry threshold.
inline bool recovery_switch(bool previous,const Parameters& p,const State& s) {
    if(p.recovery_kias<=0||s.weight_on_wheels)return false;
    const auto v=s.velocity_body-s.wind_body;
    const double kias=std::sqrt(dot(v,v))*knots_per_m_s*std::sqrt(std::max(0.,s.density)/sea_level_density);
    if(previous)return kias<p.recovery_kias+40&&flight_path_angle(s)>p.recovery_path_rad;
    return kias<p.recovery_kias&&s.pitch>p.recovery_pitch_rad;
}
// A full roll or pedal input at high alpha builds sideslip faster than the
// weaker yaw control can remove it. Scale the pilot command toward the limit.
inline double pilot_lateral_scale(const Parameters& p,double alpha_limit,double alpha,bool weight_on_wheels) {
    if(weight_on_wheels)return 1.;
    return std::clamp((alpha_limit-alpha)/std::max(1e-3,alpha_limit-p.roll_alpha_start_rad),p.high_alpha_roll_floor,1.);
}
inline double high_alpha_drag_coefficient(const Parameters& p,double mach,double alpha) {
    // Separated normal-pressure drag: normal dynamic pressure scales as
    // sin(alpha)^2; projection onto the air path adds abs(sin(alpha)).
    // The nonnegative coefficient is fitted to recordings, not an aircraft
    // value taken from another model. The measured lift curve stays separate.
    const double incidence=std::abs(std::sin(alpha-sample(p.alpha0,mach)));
    return p.high_alpha_drag*incidence*incidence*incidence;
}
inline double lift_coefficient(const Parameters& p,double mach,double alpha,double flap) {
    const double slope=sample(p.cla,mach),zero=sample(p.alpha0,mach);
    const double raw=slope*(alpha-zero)+p.flap_lift*std::clamp(flap,0.,1.);
    const double maximum=sample(p.clmax,mach),point=p.lift_break_fraction*maximum;
    if(!(maximum>0)||!(slope>0)) return 0;
    double lift=std::abs(raw);
    if(lift>point) lift=point+(maximum-point)*std::tanh((lift-point)/(maximum-point));
    const double peak_alpha=maximum/slope;
    const double excess=std::max(0.,std::abs(alpha-zero)-peak_alpha);
    lift*=std::clamp(1-p.post_stall_decrease_per_rad*excess,.1,1.);
    return std::copysign(lift,raw);
}
inline Output evaluate(const Parameters& p,const State& s,const Input& u) {
    Output o;
    if(!valid_parameters(p)||!finite(s.velocity_body)||!finite(s.wind_body)||!finite(s.angular_rate)||!finite(s.inertia)
       ||!std::isfinite(s.density)||!std::isfinite(s.sound)||!std::isfinite(s.mass)
       ||!std::isfinite(s.alpha)||!std::isfinite(s.beta)||!std::isfinite(s.pitch)
       ||!std::isfinite(s.bank)||!std::isfinite(s.normal_g)
       ||!std::isfinite(u.pitch)||!std::isfinite(u.roll)||!std::isfinite(u.pedal)
       ||!std::isfinite(u.brake)||!std::isfinite(u.gear)||!std::isfinite(u.flap)
       ||!(s.mass>0)||!(s.sound>0)||s.density<0||!(p.area_m2>0)
       ||!(s.inertia.x>0)||!(s.inertia.y>0)||!(s.inertia.z>0)) return o;
    const Vec3 air=s.velocity_body-s.wind_body;
    const double speed=std::sqrt(dot(air,air)),pressure=.5*s.density*speed*speed;
    if(!(pressure>0)||speed<1e-6) { o.valid=true;return o; }
    const double mach=speed/s.sound;
    const double cl=lift_coefficient(p,mach,s.alpha,u.flap);
    const double cd=std::max(0.,sample(p.cd0,mach)+sample(p.induced_k,mach)*cl*cl
                             +high_alpha_drag_coefficient(p,mach,s.alpha)
                             +p.dcd_brake*std::clamp(u.brake,0.,1.)
                             +p.dcd_gear*std::clamp(u.gear,0.,1.)
                             +p.dcd_flap*std::clamp(u.flap,0.,1.));
    const Vec3 along=air*(1/speed);
    Vec3 up={0,1,0}; up=up-along*dot(up,along);
    const double up_length=std::sqrt(dot(up,up));
    if(up_length<1e-9) return o;
    up=up*(1/up_length);
    const Vec3 side=cross(along,up);
    o.lift=pressure*p.area_m2*cl; o.drag=pressure*p.area_m2*cd;
    o.force=along*(-o.drag)+up*o.lift+side*(pressure*p.area_m2*p.cy_beta*s.beta);
    const double stick=std::clamp(u.pitch,-1.,1.);
    double command=stick>=0 ? 1+stick*(p.n_max-1) : 1+stick*(1-p.n_min);
    if(p.recovery_kias>0&&s.recovery_active&&!s.weight_on_wheels) {
        const double gamma=flight_path_angle(s);
        const double recovery_load=std::clamp(std::cos(gamma)+speed/gravity*.5*(p.recovery_path_rad-gamma),p.n_min,1.);
        command=std::min(command,recovery_load);
    }
    const double limit=sample(p.alpha_limit,mach);
    // A fixed achievable-load cap creates a steady load error and can prevent
    // the full-aft input from reaching the high-alpha envelope. Instead bound
    // the command by the remaining alpha-rate margin. At the limit it becomes
    // the sensed load, removing the positive load-error term from inversion.
    const double alpha_margin=speed/gravity*p.alpha_feedback_s_inv*(limit-s.alpha);
    command=std::min(command,s.normal_g+alpha_margin);
    const double path_rate=gravity/speed*(s.normal_g-std::cos(s.pitch)*std::cos(s.bank));
    double pitch_rate=gravity/speed*(command-std::cos(s.pitch)*std::cos(s.bank));
    pitch_rate=std::min(pitch_rate,path_rate+p.alpha_feedback_s_inv*(limit-s.alpha));
    const double kias=speed*knots_per_m_s*std::sqrt(s.density/sea_level_density);
    const double high_alpha_scale=pilot_lateral_scale(p,limit,s.alpha,s.weight_on_wheels);
    const double roll_rate=std::clamp(u.roll,-1.,1.)*high_alpha_scale*sample(p.roll_limit,kias);
    const double roll_acceleration=bounded((roll_rate-s.angular_rate.x)/p.tau_roll,p.control_power_per_pa.x*pressure);
    // Body axes are X forward, Y up, Z right. Rotation alone gives
    // beta_dot = r*cos(alpha) + p*sin(alpha). Include that roll coupling,
    // and the nonrotating air-velocity derivative from force and gravity.
    // Native thrust stays in the bridge; no engine model enters this law.
    const double sa=std::sin(s.alpha),ca=std::cos(s.alpha),sb=std::sin(s.beta);
    const double cb=std::max(.25,std::cos(s.beta)),safe_ca=std::max(.25,ca);
    const Vec3 acceleration=o.force*(1/s.mass)+Vec3{-gravity*std::sin(s.pitch),
        -gravity*std::cos(s.pitch)*std::cos(s.bank),gravity*std::cos(s.pitch)*std::sin(s.bank)};
    const double beta_external=(acceleration.z-sb*dot(acceleration,along))/(speed*cb);
    const double beta_rate=s.angular_rate.y*ca+s.angular_rate.x*sa+beta_external;
    const double alpha_rate=s.angular_rate.z-(sb/cb)*(s.angular_rate.x*ca-s.angular_rate.y*sa)
        +(air.y*acceleration.x-air.x*acceleration.y)/std::max(1e-6,air.x*air.x+air.y*air.y);
    const double yaw_acceleration=(-p.k_beta*s.beta-p.k_r*beta_rate
        -roll_acceleration*sa-alpha_rate*(s.angular_rate.x*ca-s.angular_rate.y*sa))/safe_ca
        +std::clamp(u.pedal,-1.,1.)*high_alpha_scale*p.pedal_rate/p.tau_yaw;
    // Released stick should damp alpha change, not oppose the pitch rate of
    // the flight path itself. Preserve the measured full-stick pull response.
    const double neutral=p.neutral_stick_width>0&&!s.weight_on_wheels
        ?std::clamp(1-std::abs(stick)/p.neutral_stick_width,0.,1.):0;
    // Released-stick trim needs stronger load-error feedback than the smooth
    // full-stick onset. Keep the selected onset values at commanded input.
    const double trim_gain=p.positive_load_limit>0?1.2*neutral:0.;
    // Keep candidate 2's measured ground pitch response through liftoff.
    const double wp=s.weight_on_wheels?3.0538127212911865:p.wp;
    const double kn=s.weight_on_wheels?1.6359128355278652:p.kn;
    const double damping=s.weight_on_wheels?15.21395727316683:p.pitch_damping;
    double pitch_acceleration=wp*(pitch_rate-s.angular_rate.z)+(kn+trim_gain)*(command-s.normal_g)
        -damping*(s.angular_rate.z-neutral*path_rate);
    double pitch_power=p.control_power_per_pa.z*pressure;
    if(p.alpha_guard_gain>0&&!s.weight_on_wheels) {
        // Active protection: anticipate the boundary using alpha rate, then
        // explicitly demand nose-down acceleration whenever above the limit.
        double guard=p.alpha_guard_gain*(p.alpha_feedback_s_inv*(limit-s.alpha)-alpha_rate);
        if(s.alpha>limit) guard=std::min(guard,-p.alpha_guard_gain*(s.alpha-limit));
        if(guard<pitch_acceleration) {
            pitch_acceleration=guard;
            pitch_power*=1+p.alpha_guard_power_gain*sa*sa;
        }
    }
    if(p.positive_load_limit>0&&!s.weight_on_wheels) {
        // Bound the measured load, not just the pilot's command. Predict the
        // load from the local lift slope over the short control response.
        // Leave the calibrated 350-knot pull below the 8 g boundary intact.
        const double load_slope=std::max(1.,pressure*p.area_m2*sample(p.cla,mach)/(s.mass*gravity));
        const double predicted_load=s.normal_g+.05*load_slope*alpha_rate;
        if(predicted_load>p.positive_load_limit)
            pitch_acceleration=std::min(pitch_acceleration,20*((p.positive_load_limit-s.normal_g)/(.05*load_slope)-alpha_rate));
        if(predicted_load<p.negative_load_limit)
            pitch_acceleration=std::max(pitch_acceleration,20*((p.negative_load_limit-s.normal_g)/(.05*load_slope)-alpha_rate));
    }
    o.wanted_acceleration={
       roll_acceleration,
       bounded(yaw_acceleration,p.control_power_per_pa.y*pressure*(1+p.yaw_alpha_power_gain*sa*sa)),
       bounded(pitch_acceleration,pitch_power)};
    const Vec3 momentum={s.inertia.x*s.angular_rate.x,s.inertia.y*s.angular_rate.y,s.inertia.z*s.angular_rate.z};
    o.moment=Vec3{s.inertia.x*o.wanted_acceleration.x,s.inertia.y*o.wanted_acceleration.y,s.inertia.z*o.wanted_acceleration.z}+cross(s.angular_rate,momentum);
    o.force={bounded(o.force.x,p.force_bound_n),bounded(o.force.y,p.force_bound_n),bounded(o.force.z,p.force_bound_n)};
    o.moment={bounded(o.moment.x,p.moment_bound_nm),bounded(o.moment.y,p.moment_bound_nm),bounded(o.moment.z,p.moment_bound_nm)};
    o.load_command=command;o.dynamic_pressure=pressure;
    o.valid=finite(o.force)&&finite(o.moment)&&finite(o.wanted_acceleration)
       &&std::isfinite(o.lift)&&std::isfinite(o.drag)&&std::isfinite(o.load_command)&&std::isfinite(o.dynamic_pressure);
    if(!o.valid) return {};
    return o;
}
// Rate-limited presentation only; this state never enters evaluate().
inline double surface_position(double previous,double command,double gain,double rate,double dt) {
    if(!std::isfinite(previous)||!std::isfinite(command)||!std::isfinite(gain)
       ||!std::isfinite(rate)||!std::isfinite(dt)||dt<0||rate<0) return 0;
    return std::clamp(previous+std::clamp(command*gain-previous,-rate*dt,rate*dt),-1.,1.);
}
} // namespace f23b::flight_core
