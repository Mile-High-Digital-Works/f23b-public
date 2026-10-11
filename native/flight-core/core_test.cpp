// SPDX-License-Identifier: MIT
// Mathematical fixtures, not selected aircraft values or a performance pass.
#include "core.hpp"
#include "fitted_parameters.hpp"
#include "recorded_trim.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace f23b::flight_core;
void require(bool value,const char* name) { if(!value) throw std::runtime_error(name); }
bool near(double a,double b,double tolerance=1e-7) { return std::abs(a-b)<tolerance; }
Parameters fixture() {
    Parameters p;
    for(std::size_t i=0;i<12;++i) {
        const double mach=.2+2.4*static_cast<double>(i)/11;
        p.cla.x[i]=p.alpha0.x[i]=p.cd0.x[i]=p.induced_k.x[i]=p.clmax.x[i]=p.alpha_limit.x[i]=mach;
        p.cla.y[i]=4;p.alpha0.y[i]=0;p.cd0.y[i]=.02;p.induced_k.y[i]=.1;p.clmax.y[i]=2;p.alpha_limit.y[i]=.4;
    }
    p.roll_limit={{{100,500}},{{3,2}}};p.area_m2=65;p.n_max=8;p.n_min=-1;
    p.wp=2;p.kn=.2;p.tau_roll=.1;p.tau_yaw=.25;p.k_beta=8;p.k_r=5;p.cy_beta=-.5;p.pedal_rate=.2;
    p.control_power_per_pa={.002,.002,.002};p.lift_break_fraction=.8;p.post_stall_decrease_per_rad=.1;
    p.alpha_feedback_s_inv=2;p.force_bound_n=2500000;p.moment_bound_nm=2500000;
    return p;
}
State state() { State s;s.velocity_body={200,-20,8};s.density=1.2;s.sound=340;s.mass=19327;
    s.inertia={31087,240989,168771};s.alpha=.1;s.beta=.04;s.pitch=.1;s.bank=.1;s.normal_g=1;return s; }
int main() {
    auto p=fixture();auto s=state();Input u{.2,.4,.6,0,0,0};
    auto separated=p;separated.high_alpha_drag=1.5;
    require(near(high_alpha_drag_coefficient(separated,.5,0),0),"no separated drag at zero incidence");
    require(near(high_alpha_drag_coefficient(separated,.5,.6),high_alpha_drag_coefficient(separated,.5,-.6)),"separated drag incidence symmetry");
    auto drag_on=evaluate(separated,s,u),drag_off=evaluate(p,s,u);
    require(drag_on.drag>drag_off.drag&&near(drag_on.lift,drag_off.lift),"added separated drag preserves lift");
    require(dot(drag_on.force-drag_off.force,s.velocity_body-s.wind_body)<0,"separated drag removes energy");
    auto still=s;still.velocity_body={};still.weight_on_wheels=true;
    auto zero=evaluate(p,still,u);require(zero.valid&&near(dot(zero.force,zero.force),0)
      &&near(dot(zero.moment,zero.moment),0)&&near(dot(zero.wanted_acceleration,zero.wanted_acceleration),0),"zero speed/ground rest");
    s.angular_rate={.05,.02,.03};auto positive=evaluate(p,s,u);
    auto mirror=s;mirror.velocity_body.z=-s.velocity_body.z;mirror.beta=-s.beta;mirror.bank=-s.bank;
    mirror.angular_rate.x=-s.angular_rate.x;mirror.angular_rate.y=-s.angular_rate.y;
    auto opposite=evaluate(p,mirror,Input{u.pitch,-u.roll,-u.pedal,0,0,0});
    require(positive.valid&&opposite.valid&&near(positive.force.x,opposite.force.x)
      &&near(positive.force.y,opposite.force.y)&&near(positive.force.z,-opposite.force.z)
      &&near(positive.moment.x,-opposite.moment.x)&&near(positive.moment.y,-opposite.moment.y)
      &&near(positive.moment.z,opposite.moment.z),"lateral symmetry");
    Vec3 momentum={s.inertia.x*s.angular_rate.x,s.inertia.y*s.angular_rate.y,s.inertia.z*s.angular_rate.z};
    auto torque=positive.moment-cross(s.angular_rate,momentum);
    require(near(torque.x/s.inertia.x,positive.wanted_acceleration.x)
      &&near(torque.y/s.inertia.y,positive.wanted_acceleration.y)
      &&near(torque.z/s.inertia.z,positive.wanted_acceleration.z),"inertia and gyroscopic cancellation");
    auto extreme=s;extreme.velocity_body={10000,-1000,500};extreme.angular_rate={100,100,100};
    auto bounded_result=evaluate(p,extreme,Input{1,1,1,1,1,1});require(bounded_result.valid,"finite extreme output");
    for(double v:{bounded_result.force.x,bounded_result.force.y,bounded_result.force.z})require(std::abs(v)<=p.force_bound_n,"force bound");
    for(double v:{bounded_result.moment.x,bounded_result.moment.y,bounded_result.moment.z})require(std::abs(v)<=p.moment_bound_nm,"moment bound");
    auto invalid=s;invalid.velocity_body.x=std::numeric_limits<double>::quiet_NaN();
    require(!evaluate(p,invalid,u).valid,"reject nonfinite state");
    auto invalid_p=p;invalid_p.cla.y[3]=std::numeric_limits<double>::quiet_NaN();
    require(!evaluate(invalid_p,s,u).valid,"reject nonfinite calibration");
    invalid_p=p;invalid_p.control_power_per_pa.x=-1;
    require(!evaluate(invalid_p,s,u).valid,"reject negative power");
    invalid_p=p;invalid_p.high_alpha_drag=-1;
    require(!evaluate(invalid_p,s,u).valid,"reject negative separated drag");
    for(double mach:p.cla.x) {
        auto trim=state();trim.beta=0;trim.bank=0;trim.angular_rate={};
        const double speed=mach*trim.sound,pressure=.5*trim.density*speed*speed;
        const double cl=trim.mass*gravity/(pressure*p.area_m2);
        if(cl>.8*sample(p.clmax,mach))continue;
        trim.alpha=cl/sample(p.cla,mach);trim.pitch=trim.alpha;
        trim.velocity_body={speed*std::cos(trim.alpha),-speed*std::sin(trim.alpha),0};
        auto result=evaluate(p,trim,{});
        require(result.valid&&near(result.lift,trim.mass*gravity,1e-5),"wind-axis level trim at each usable Mach");
    }
    require(near(surface_position(0,1,1,.5,.2),.1)&&near(surface_position(0,-1,1,.5,.2),-.1),"surface gain/rate symmetry");
    require(near(surface_position(0,1,1,.5,-1),0),"invalid surface step");
    // These are recorded pressure/mass/alpha triples, not values generated by
    // this model. They test the final fit at each populated baseline Mach band.
    auto fitted=fitted_parameters();
    require(valid_parameters(fitted),"final candidate parameters valid");
    if(fitted.alpha_guard_gain>0) {
        for(double speed:{35.,60.,100.}) for(double alpha:{.90,1.05}) {
            auto high=state();high.alpha=alpha;high.pitch=.2;high.bank=0;high.beta=0;
            high.velocity_body={speed*std::cos(alpha),-speed*std::sin(alpha),0};
            high.angular_rate={0,0,-.1};
            auto protected_output=evaluate(fitted,high,Input{1,0,0,0,0,0});
            require(protected_output.valid&&protected_output.wanted_acceleration.z<0,"above limit actively commands nose down even with full aft stick");
            high.angular_rate={0,.5,0};
            auto yaw=evaluate(fitted,high,{});
            require(yaw.wanted_acceleration.y<-.5,"high-alpha low-speed yaw damping remains effective");
            high.angular_rate={};
            auto roll=evaluate(fitted,high,Input{0,1,0,0,0,0});
            require(roll.wanted_acceleration.x>0&&roll.wanted_acceleration.y<0,"high-alpha roll couples to compensating yaw acceleration");
        }
    }
    for(const auto& row:recorded_trim_cases()) {
        auto measured=state();measured.beta=0;measured.bank=0;measured.angular_rate={};
        const double mach=row[0],pressure=row[1],speed=mach*measured.sound;
        measured.density=2*pressure/(speed*speed);measured.mass=row[2];
        measured.alpha=row[3];measured.pitch=measured.alpha;
        measured.velocity_body={speed*std::cos(measured.alpha),-speed*std::sin(measured.alpha),0};
        auto predicted=evaluate(fitted,measured,{});
        require(predicted.valid&&std::abs(predicted.force.y/(measured.mass*gravity)-1)<.2,"fitted recorded-Mach trim load");
        require(std::abs(predicted.wanted_acceleration.z)<.01,"fitted neutral trim pitch acceleration");
    }
    auto stopped=state();stopped.velocity_body={};stopped.weight_on_wheels=true;
    auto final_zero=evaluate(fitted,stopped,Input{1,1,1,1,1,1});
    require(final_zero.valid&&near(dot(final_zero.force,final_zero.force),0)
        &&near(dot(final_zero.wanted_acceleration,final_zero.wanted_acceleration),0),"fitted ground-rest protection");
    std::cout<<"CORE UNIT PASS: zero speed, ground rest, trim, symmetry, bounds, finite guards, gyroscopic inversion, visual rate limit\n";
}
