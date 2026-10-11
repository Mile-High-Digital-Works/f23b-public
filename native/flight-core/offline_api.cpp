// SPDX-License-Identifier: MIT
// Stateless recorded-state probe and six-axis offline card integrator.
#include "core.hpp"
#include "fitted_parameters.hpp"
#include "tuning.hpp"
using namespace f23b::flight_core;
#define API extern "C" __declspec(dllexport)
struct Quaternion {
    double w,x,y,z;
    Quaternion operator*(Quaternion b) const {
        return {w*b.w-x*b.x-y*b.y-z*b.z,w*b.x+x*b.w+y*b.z-z*b.y,
          w*b.y-x*b.z+y*b.w+z*b.x,w*b.z+x*b.y-y*b.x+z*b.w};
    }
    Quaternion inverse() const {return {w,-x,-y,-z};}
    Vec3 turn(Vec3 a) const {auto b=(*this)*Quaternion{0,a.x,a.y,a.z}*inverse();return {b.x,b.y,b.z};}
};
static auto parameters=fitted_parameters();
API int configure_tuning_json(const char* text) {
    parameters=fitted_parameters();std::vector<std::string> log;
    return tuning::apply(text,parameters,log)?1:0;
}
// Offline fitter only. Production evaluate() remains a pure function.
API void fit_controls(const double* v) {
    parameters.n_max=v[0];parameters.wp=v[1];parameters.kn=v[2];parameters.pitch_damping=v[3];
    parameters.k_beta=v[4];parameters.k_r=v[5];parameters.cy_beta=v[6];
    parameters.control_power_per_pa.z=v[7];parameters.flap_lift=v[8];
    parameters.dcd_gear=v[9];parameters.dcd_flap=v[10];
    parameters.alpha_feedback_s_inv=v[11];
}
// Input: body V[3], wind[3], omega[3], I[3],rho,sound,mass,alpha,beta,
// pitch,bank,normal_g,pilot[6],thrust,air-path-sin,cg_y,cg_z,thrust_R-minus-L.
// Output: force[3], moment[3], angular_accel[3], path_accel, normal_g,valid.
API void recorded_states(const double* a,int count,double* out) {
    for(int j=0;j<count;++j,a+=31,out+=12) {
        State s{{a[0],a[1],a[2]},{a[3],a[4],a[5]},
          {a[6],a[7],a[8]},{a[9],a[10],a[11]},a[12],a[13],a[14],a[15],a[16],a[17],a[18],a[19]};
        Input u{a[20],a[21],a[22],a[23],a[24],a[25]};
        auto o=evaluate(parameters,s,u);
        // Retained straight native-engine axes, from project propulsion.hpp.
        Vec3 thrust_moment{0,1.311020017*a[30]-a[29]*a[26],(a[28]+.320670754)*a[26]};
        auto torque=o.moment+thrust_moment-cross(s.angular_rate,{s.inertia.x*s.angular_rate.x,s.inertia.y*s.angular_rate.y,s.inertia.z*s.angular_rate.z});
        auto air=s.velocity_body-s.wind_body;const double v=std::sqrt(dot(air,air));
        out[0]=o.force.x;out[1]=o.force.y;out[2]=o.force.z;
        out[3]=o.moment.x;out[4]=o.moment.y;out[5]=o.moment.z;
        out[6]=torque.x/s.inertia.x;out[7]=torque.y/s.inertia.y;out[8]=torque.z/s.inertia.z;
        out[9]=v>0?dot(o.force+Vec3{a[26],0,0},air)/(s.mass*v)-gravity*a[27]:0;
        out[10]=o.force.y/(s.mass*gravity);out[11]=o.valid?1:0;
    }
}
// y: quaternion[4], world V[3], body omega[3], altitude.
// env: rho,sound,mass,T,I[3],world wind[3],cg_y,cg_z,thrust_R-minus-L.
// output: IAS,TAS,M,alt_ft,alpha,beta,g,p,r,q,valid,pitch,bank,heading,
// world V[3],ncmd,pressure,F[3],gx,gz,path_accel,angular_accel[3].
void card_step_impl(double* y,const double* e,const double* u,double dt,double* out,bool* recovery) {
    const int count=dt>0?std::max(1,static_cast<int>(std::ceil(dt/.006))):0;
    for(int j=0;j<=count;++j) {
        Quaternion rotation{y[0],y[1],y[2],y[3]};
        Vec3 velocity{y[4],y[5],y[6]},omega{y[7],y[8],y[9]};
        State s;s.velocity_body=rotation.inverse().turn(velocity);
        s.wind_body=rotation.inverse().turn({e[7],e[8],e[9]});
        s.angular_rate=omega;s.inertia={e[4],e[5],e[6]};s.density=e[0];s.sound=e[1];s.mass=e[2];
        auto air=s.velocity_body-s.wind_body;const double v=std::sqrt(dot(air,air));
        s.alpha=std::atan2(-air.y,air.x);s.beta=v>1e-9?std::asin(std::clamp(air.z/v,-1.,1.)):0;
        auto fwd=rotation.turn({1,0,0}),up=rotation.turn({0,1,0}),right=rotation.turn({0,0,1});
        s.pitch=std::asin(std::clamp(fwd.y,-1.,1.));s.bank=std::atan2(-right.y,up.y);
        if(recovery){*recovery=recovery_switch(*recovery,parameters,s);s.recovery_active=*recovery;}
        Input input{u[0],u[1],u[2],u[3],u[4],u[5]};s.normal_g=1;
        auto o=evaluate(parameters,s,input);s.normal_g=o.force.y/(s.mass*gravity);o=evaluate(parameters,s,input);
        Vec3 thrust_moment{0,1.311020017*e[12]-e[11]*e[3],(e[10]+.320670754)*e[3]};
        auto torque=o.moment+thrust_moment-cross(omega,{s.inertia.x*omega.x,s.inertia.y*omega.y,s.inertia.z*omega.z});
        Vec3 rate_dot{torque.x/s.inertia.x,torque.y/s.inertia.y,torque.z/s.inertia.z};
        Vec3 acceleration=rotation.turn(o.force+Vec3{e[3],0,0})*(1/s.mass)+Vec3{0,-gravity,0};
        if(j<count) {
            const double h=dt/count;auto d=rotation*Quaternion{0,omega.x,omega.y,omega.z};
            y[0]+=.5*h*d.w;y[1]+=.5*h*d.x;y[2]+=.5*h*d.y;y[3]+=.5*h*d.z;
            double n=std::sqrt(y[0]*y[0]+y[1]*y[1]+y[2]*y[2]+y[3]*y[3]);
            for(int k=0;k<4;++k)y[k]/=n;
            y[10]+=h*velocity.y;y[4]+=h*acceleration.x;y[5]+=h*acceleration.y;y[6]+=h*acceleration.z;
            y[7]+=h*rate_dot.x;y[8]+=h*rate_dot.y;y[9]+=h*rate_dot.z;
        } else {
            const double deg=180/3.141592653589793;
            const double values[]={v*knots_per_m_s*std::sqrt(s.density/sea_level_density),v*knots_per_m_s,
              v/s.sound,y[10]/.3048,s.alpha*deg,s.beta*deg,s.normal_g,omega.x*deg,omega.y*deg,omega.z*deg,
              o.valid?1.:0.,s.pitch*deg,s.bank*deg,std::atan2(fwd.z,fwd.x)*deg,
              velocity.x,velocity.y,velocity.z,o.load_command,o.dynamic_pressure,o.force.x,o.force.y,o.force.z,
              (o.force.x+e[3])/(s.mass*gravity),o.force.z/(s.mass*gravity),
              v>0?dot(acceleration,rotation.turn(air))/v:0,rate_dot.x,rate_dot.y,rate_dot.z};
            std::copy(std::begin(values),std::end(values),out);
        }
    }
}
API void card_step(double* y,const double* e,const double* u,double dt,double* out) {
    card_step_impl(y,e,u,dt,out,nullptr);
}
API void card_step_development(double* y,const double* e,const double* u,double dt,double* out,bool* recovery) {
    if(dt==0)*recovery=false;
    card_step_impl(y,e,u,dt,out,recovery);
}
