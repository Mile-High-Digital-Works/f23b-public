// SPDX-License-Identifier: MIT
#include "tuning.hpp"
#include "fitted_parameters.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace f23b::flight_core;
void check(bool condition,const char* text){if(!condition)throw std::runtime_error(text);}
int main() {
    const auto defaults=fitted_parameters();auto p=defaults;std::vector<std::string> log;
    check(!tuning::load("__no_such_f23b_tuning_directory__/F23B-tuning.json",p,log),"absent file");
    check(std::memcmp(&p,&defaults,sizeof(p))==0,"absent file preserves parameter bytes");
    for(int i=0;i<600;++i) {
        State s;s.velocity_body={50.+i,-10,2};s.inertia={31087,240989,168771};s.density=.7;
        s.sound=320;s.mass=19000;s.alpha=.1;s.beta=.02;s.normal_g=1;s.angular_rate={.03,.02,.01};
        Input u{double(i%21)/10-1,.3,-.2,0,0,0};
        const auto a=evaluate(defaults,s,u),b=evaluate(p,s,u);
        check(std::memcmp(&a.force,&b.force,sizeof(Vec3))==0&&std::memcmp(&a.moment,&b.moment,sizeof(Vec3))==0
            &&std::memcmp(&a.wanted_acceleration,&b.wanted_acceleration,sizeof(Vec3))==0
            &&a.load_command==b.load_command&&a.lift==b.lift&&a.drag==b.drag&&a.valid==b.valid,"absent file preserves output bytes");
    }
    log.clear();
    check(tuning::apply(R"({"tau_roll":1000,"pitch_wp":-20,"unrecognized":{"nested":[1,true,null]},"c_pitch":0.002})",p,log),"valid JSON");
    check(p.tau_roll==.4&&p.wp==.5&&p.control_power_per_pa.z==.0003,"outside values clamped");
    check(std::any_of(log.begin(),log.end(),[](const auto& x){return x.find("ignored unknown key: unrecognized")!=std::string::npos;}),"unknown key logged");
    check(log.size()==4,"every applied override logged");
    for(const auto* invalid:{"", "{", "{\"tau_roll\":1,}","{\"tau_roll\":.2}","{\"tau_roll\":01}",
                            "{\"tau_roll\":NaN}","{\"tau_roll\":1,\"tau_roll\":2}","{} garbage"}) {
        p=defaults;log.clear();check(!tuning::apply(invalid,p,log),"malformed JSON refused");
        check(std::memcmp(&p,&defaults,sizeof(p))==0,"malformed file preserves defaults");
    }
    p=defaults;log.clear();check(tuning::apply(R"({"tau_roll":1e999,"pitch_wp":"three"})",p,log),"unusable values ignored");
    check(std::memcmp(&p,&defaults,sizeof(p))==0,"nonfinite/nonnumeric values preserve defaults");
    p=defaults;log.clear();f23b::DryThrustSettings dry;
    check(tuning::apply(R"({"dry_gain_end":1.5,"preset":"speed-1"})",p,log,&dry)&&dry.enabled&&dry.end_gain==1.5,"speed preset with order-independent overrides");
    check(std::memcmp(&p,&defaults,sizeof(p))==0,"speed preset does not change aerodynamics");
    p=defaults;check(tuning::apply(R"({"preset":"protect-1"})",p,log),"protect preset");
    State zoom;zoom.density=.8;zoom.velocity_body={150,0,0};zoom.pitch=1.2;
    check(recovery_switch(false,p,zoom),"nose-high slow state starts recovery");
    zoom.pitch=.3;check(recovery_switch(true,p,zoom)&&!recovery_switch(false,p,zoom),"recovery hysteresis");
    zoom.pitch=-.2;check(!recovery_switch(true,p,zoom),"nose-low path releases recovery");
    zoom.pitch=1.2;zoom.weight_on_wheels=true;check(!recovery_switch(true,p,zoom),"ground inhibits recovery");
    zoom.weight_on_wheels=false;check(!recovery_switch(true,defaults,zoom),"no preset means no recovery controller");
    std::cout<<"TUNING PASS: absent byte identity, clamps, unknown keys, JSON validation, finite guards\n";
}
