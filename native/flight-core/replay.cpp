// SPDX-License-Identifier: MIT
// Free six-axis replay. Only the first row supplies position, velocity and rates.
#include "core.hpp"
#include "fitted_parameters.hpp"
#ifdef INSTALLED_CONTROL
#include "installed_control.hpp"
#endif
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>
using namespace f23b::flight_core;
struct Rotation {
    double w{1},x{},y{},z{};
    Rotation operator*(Rotation b) const {
        return {w*b.w-x*b.x-y*b.y-z*b.z,w*b.x+x*b.w+y*b.z-z*b.y,
                w*b.y-x*b.z+y*b.w+z*b.x,w*b.z+x*b.y-y*b.x+z*b.w};
    }
    Rotation conjugate() const { return {w,-x,-y,-z}; }
    Vec3 turn(Vec3 v) const { auto r=(*this)*Rotation{0,v.x,v.y,v.z}*conjugate();return {r.x,r.y,r.z}; }
    void advance(Vec3 omega,double dt) {
        auto d=(*this)*Rotation{0,omega.x,omega.y,omega.z};
        w+=.5*dt*d.w;x+=.5*dt*d.x;y+=.5*dt*d.y;z+=.5*dt*d.z;
        const double n=std::sqrt(w*w+x*x+y*y+z*z);w/=n;x/=n;y/=n;z/=n;
    }
    static Rotation initial(double pitch,double bank,double heading) {
        return Rotation{std::cos(heading/2),0,-std::sin(heading/2),0}
             *Rotation{std::cos(pitch/2),0,0,std::sin(pitch/2)}
             *Rotation{std::cos(bank/2),std::sin(bank/2),0,0};
    }
};
struct Row {
    // Columns: time,rho,sound,mass,thrust,pilot_pitch,pilot_roll,pedal,brake,
    // gear,flap,world_vx,world_vy,world_vz,wx,wy,wz,pitch,bank,heading,p,r,q,
    // altitude,ix,iy,iz. Attitude and velocity after row 1 are reference only.
    std::array<double,27> v{};
};
int main(int argc,char**argv) {
    if(argc!=3) return 2;
    std::ifstream in(argv[1]);std::ofstream out(argv[2]);
    if(!in||!out) return 2;
    std::string line;std::getline(in,line);std::vector<Row> rows;
    while(std::getline(in,line)) {
        std::istringstream s(line);std::string cell;Row r;
        for(auto&v:r.v) { if(!std::getline(s,cell,',')) throw std::runtime_error("short row");v=std::stod(cell); }
        rows.push_back(r);
    }
    if(rows.size()<2) return 2;
    const auto parameters=fitted_parameters();
    auto initial=rows.front().v;
    Rotation rotation=Rotation::initial(initial[17],initial[18],initial[19]);
    Vec3 velocity{initial[11],initial[12],initial[13]},omega{initial[20],initial[21],initial[22]};
    double altitude=initial[23];
    bool all_valid=true;
#ifdef INSTALLED_CONTROL
    InstalledControl installed;
#endif
    out<<"time,ias_kt,tas_kt,mach,alt_ft,aoa_deg,slip_deg,g,p_dps,r_dps,q_dps,valid,pitch_deg,bank_deg,heading_deg,vx,vy,vz,n_cmd,pressure_pa,fx,fy,fz\n"<<std::setprecision(15);
    auto observe=[&](const std::array<double,27>& a,double dt) {
        State state;state.velocity_body=rotation.conjugate().turn(velocity);
        state.wind_body=rotation.conjugate().turn({a[14],a[15],a[16]});
        state.angular_rate=omega;state.inertia={a[24],a[25],a[26]};
        state.density=a[1];state.sound=a[2];state.mass=a[3];
        const Vec3 air=state.velocity_body-state.wind_body;
        const double speed=std::sqrt(dot(air,air));
        state.alpha=std::atan2(-air.y,air.x);
        state.beta=speed>1e-6 ? std::asin(std::clamp(air.z/speed,-1.,1.)) : 0;
        auto fwd=rotation.turn({1,0,0}),up=rotation.turn({0,1,0}),right=rotation.turn({0,0,1});
        state.pitch=std::asin(std::clamp(fwd.y,-1.,1.));state.bank=std::atan2(-right.y,up.y);
        Input input{a[5],a[6],a[7],a[8],a[9],a[10]};
        // The sensed normal acceleration is the aerodynamic body-Y force.
        state.normal_g=1;
#ifdef INSTALLED_CONTROL
        auto result=installed.step(state,input,altitude,dt);
        state.normal_g=result.force.y/(state.mass*gravity);
#else
        auto sensed=evaluate(parameters,state,input);
        state.normal_g=sensed.force.y/(state.mass*gravity);
        auto result=evaluate(parameters,state,input);
#endif
        all_valid=all_valid&&result.valid;
        if(dt>0&&result.valid) {
            Vec3 acceleration=rotation.turn(result.force+Vec3{a[4],0,0})*(1/state.mass)+Vec3{0,-gravity,0};
            Vec3 angular_momentum={state.inertia.x*omega.x,state.inertia.y*omega.y,state.inertia.z*omega.z};
            Vec3 torque=result.moment-cross(omega,angular_momentum);
            Vec3 rate_change={torque.x/state.inertia.x,torque.y/state.inertia.y,torque.z/state.inertia.z};
            altitude+=dt*velocity.y;velocity=velocity+acceleration*dt;
            rotation.advance(omega,dt);omega=omega+rate_change*dt;
        }
        return std::array<double,23>{a[0],speed*knots_per_m_s*std::sqrt(a[1]/sea_level_density),
          speed*knots_per_m_s,speed/a[2],altitude/.3048,state.alpha*180/3.141592653589793,
          state.beta*180/3.141592653589793,state.normal_g,omega.x*180/3.141592653589793,
          omega.y*180/3.141592653589793,omega.z*180/3.141592653589793,all_valid?1.:0.,
          state.pitch*180/3.141592653589793,state.bank*180/3.141592653589793,
          std::atan2(fwd.z,fwd.x)*180/3.141592653589793,velocity.x,velocity.y,velocity.z,
          result.load_command,result.dynamic_pressure,result.force.x,result.force.y,result.force.z};
    };
    for(std::size_t i=0;i<rows.size();++i) {
        if(i>0) {
            const double span=rows[i].v[0]-rows[i-1].v[0];
            if(!(span>0)) return 3;
            // Follow the recorded 0.006-s game step, including long-frame substeps.
            const int n=std::max(1,static_cast<int>(std::ceil(span/.006)));
            for(int k=0;k<n;++k) {
                const double fraction=(k+.5)/n;std::array<double,27>a{};
                for(std::size_t j=0;j<a.size();++j) a[j]=rows[i-1].v[j]+fraction*(rows[i].v[j]-rows[i-1].v[j]);
                observe(a,span/n);
            }
        }
        auto result=observe(rows[i].v,0);
        for(std::size_t j=0;j<result.size();++j) out<<(j?",":"")<<result[j];out<<'\n';
    }
    std::cout<<"FREE REPLAY "<<rows.size()<<" rows\n";
}
