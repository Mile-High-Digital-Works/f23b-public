// SPDX-License-Identifier: MIT
// Startup-only development settings. No I/O or parsing enters evaluate().
#pragma once
#include "core.hpp"
#include "development_options.hpp"
#include <charconv>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace f23b::flight_core::tuning {
struct Value { enum Kind { number, text, other } kind{other}; double scalar{}; std::string string; };
struct Entry { std::string key; Value value; };
class Json {
    std::string_view source; std::size_t position{};
    void space() { while(position<source.size()&&std::isspace(static_cast<unsigned char>(source[position])))++position; }
    bool take(char c) { space();if(position<source.size()&&source[position]==c){++position;return true;}return false; }
    bool string(std::string& out) {
        if(!take('"'))return false;
        while(position<source.size()) {
            char c=source[position++];if(c=='"')return true;
            if(static_cast<unsigned char>(c)<32)return false;
            if(c=='\\') {
                if(position>=source.size())return false;c=source[position++];
                if(c=='u') {
                    unsigned code=0;
                    for(int i=0;i<4;++i) {
                        if(position>=source.size())return false;
                        const char h=source[position++];unsigned digit;
                        if(h>='0'&&h<='9')digit=h-'0';
                        else if(h>='a'&&h<='f')digit=h-'a'+10;
                        else if(h>='A'&&h<='F')digit=h-'A'+10;
                        else return false;
                        code=code*16+digit;
                    }
                    c=code<128?static_cast<char>(code):'?';
                } else if(c=='n')c='\n';else if(c=='r')c='\r';else if(c=='t')c='\t';
                else if(c=='b')c='\b';else if(c=='f')c='\f';
                else if(c!='"'&&c!='\\'&&c!='/')return false;
            }
            out+=c;
        }
        return false;
    }
    bool value(Value& out,int depth=0) {
        space();if(position>=source.size()||depth>12)return false;
        if(source[position]=='"'){out.kind=Value::text;return string(out.string);}
        if(source[position]=='{'||source[position]=='[') {
            const bool object=source[position++]=='{';const char close=object?'}':']';
            if(take(close))return true;
            do {
                if(object){std::string key;if(!string(key)||!take(':'))return false;}
                Value ignored;if(!value(ignored,depth+1))return false;
                if(take(close))return true;
            }while(take(','));return false;
        }
        for(auto literal:{"true","false","null"}) {
            const std::string_view token=literal;
            if(source.substr(position,token.size())==token){position+=token.size();return true;}
        }
        const auto start=position;
        if(source[position]=='-')++position;
        if(position>=source.size())return false;
        if(source[position]=='0')++position;
        else {
            if(source[position]<'1'||source[position]>'9')return false;
            while(position<source.size()&&source[position]>='0'&&source[position]<='9')++position;
        }
        if(position<source.size()&&source[position]=='.') {
            const auto first=++position;
            while(position<source.size()&&source[position]>='0'&&source[position]<='9')++position;
            if(position==first)return false;
        }
        if(position<source.size()&&(source[position]=='e'||source[position]=='E')) {
            ++position;if(position<source.size()&&(source[position]=='+'||source[position]=='-'))++position;
            const auto first=position;
            while(position<source.size()&&source[position]>='0'&&source[position]<='9')++position;
            if(position==first)return false;
        }
        out.kind=Value::number;out.scalar=std::numeric_limits<double>::quiet_NaN();
        const auto result=std::from_chars(source.data()+start,source.data()+position,out.scalar);
        return result.ec==std::errc{}||result.ec==std::errc::result_out_of_range;
    }
public:
    explicit Json(std::string_view text):source(text){}
    bool object(std::vector<Entry>& entries) {
        if(source.size()>65536||!take('{'))return false;
        std::set<std::string> keys;
        if(!take('}')) {
            do {
                Entry entry;
                if(entries.size()>=128||!string(entry.key)||!keys.insert(entry.key).second||!take(':')||!value(entry.value))return false;
                entries.push_back(entry);
                if(take('}'))break;
                if(!take(','))return false;
            }while(true);
        }
        space();return position==source.size();
    }
};

struct Rule { const char* key; double low,high; double Parameters::*member; };
inline const Rule rules[]={
    {"n_max",5,11.131506452745803,&Parameters::n_max},{"n_min",-2,-.5,&Parameters::n_min},
    {"pitch_wp",.5,6,&Parameters::wp},{"pitch_kn",.25,4,&Parameters::kn},
    {"pitch_damping",4,25,&Parameters::pitch_damping},{"tau_roll",.025,.4,&Parameters::tau_roll},
    {"k_beta",16,100,&Parameters::k_beta},{"k_r",8,30,&Parameters::k_r},
    {"cy_beta",-1,-.25,&Parameters::cy_beta},{"tau_yaw",.15,.5,&Parameters::tau_yaw},
    {"pedal_rate",.1,.5,&Parameters::pedal_rate},{"yaw_alpha_power_gain",1,5,&Parameters::yaw_alpha_power_gain},
    {"alpha_feedback",.5,2,&Parameters::alpha_feedback_s_inv},{"alpha_guard_gain",2,10,&Parameters::alpha_guard_gain},
    {"alpha_guard_power_gain",1,5,&Parameters::alpha_guard_power_gain},{"neutral_stick_width",.1,.3,&Parameters::neutral_stick_width},
    {"high_alpha_drag",0,8,&Parameters::high_alpha_drag},{"dcd_gear",.15,.28,&Parameters::dcd_gear},
    {"dcd_flap",.015,.05,&Parameters::dcd_flap},{"flap_lift",0,.03,&Parameters::flap_lift},
    {"dcd_brake",.05,.12,&Parameters::dcd_brake},
    {"recovery_kias",200,280,&Parameters::recovery_kias},
};
inline double clamp_value(const Entry& e,double low,double high,std::vector<std::string>& log) {
    const double result=std::clamp(e.value.scalar,low,high);
    std::ostringstream line;line.precision(17);line<<"applied "<<e.key<<'='<<result;
    if(result!=e.value.scalar)line<<" (clamped from "<<e.value.scalar<<')';
    log.push_back(line.str());return result;
}
inline bool apply(const std::string& text,Parameters& parameters,std::vector<std::string>& log,DryThrustSettings* dry=nullptr) {
    std::vector<Entry> entries;
    if(!Json(text).object(entries)){log.push_back("invalid JSON; candidate-2 defaults retained");return false;}
    Parameters next=parameters;
    DryThrustSettings dry_next=dry?*dry:DryThrustSettings{};
    // Select the preset first so explicit scalar overrides have the same
    // meaning regardless of JSON member order. Defaults never enter this path.
    for(const auto& e:entries) if(e.key=="preset") {
        if(e.value.kind==Value::text&&e.value.string=="stable-1") {
            next.wp=6.;next.kn=.44;next.pitch_damping=5.2;
            for(auto& v:next.roll_limit.y)v*=.765;
            log.push_back("preset stable-1");
        } else if(e.value.kind==Value::text&&e.value.string=="speed-1") {
            dry_next.enabled=true;log.push_back("preset speed-1");
        } else if(e.value.kind==Value::text&&e.value.string=="protect-1") {
            next.recovery_kias=260;next.recovery_pitch_rad=45*3.141592653589793/180;
            next.recovery_path_rad=-5*3.141592653589793/180;log.push_back("preset protect-1");
        } else log.push_back(e.value.kind==Value::text&&e.value.string=="default"?"preset default":"ignored unknown preset");
    }
    for(const auto& e:entries) {
        if(e.key=="preset") {
            continue;
        }
        if(e.key=="recovery_pitch_deg"||e.key=="recovery_path_deg") {
            if(e.value.kind!=Value::number||!std::isfinite(e.value.scalar)){log.push_back("ignored nonfinite or nonnumeric value: "+e.key);continue;}
            if(e.key=="recovery_pitch_deg")next.recovery_pitch_rad=clamp_value(e,30,60,log)*3.141592653589793/180;
            else next.recovery_path_rad=clamp_value(e,-10,-2,log)*3.141592653589793/180;
            continue;
        }
        if(e.key=="dry_gain_start_mach"||e.key=="dry_gain_end_mach"||e.key=="dry_gain_end") {
            if(e.value.kind!=Value::number||!std::isfinite(e.value.scalar)){log.push_back("ignored nonfinite or nonnumeric value: "+e.key);continue;}
            if(e.key=="dry_gain_start_mach")dry_next.start_mach=clamp_value(e,1.3,1.5,log);
            if(e.key=="dry_gain_end_mach")dry_next.end_mach=clamp_value(e,1.8,2.3,log);
            if(e.key=="dry_gain_end")dry_next.end_gain=clamp_value(e,1.,2.,log);
            continue;
        }
        const auto rule=std::find_if(std::begin(rules),std::end(rules),[&](const Rule& r){return e.key==r.key;});
        const bool table=e.key=="roll_rate_scale"||e.key=="cd0_scale"||e.key=="induced_k_scale"||e.key=="alpha_limit_deg"||e.key=="c_pitch";
        if(rule==std::end(rules)&&!table){log.push_back("ignored unknown key: "+e.key);continue;}
        if(e.value.kind!=Value::number||!std::isfinite(e.value.scalar)){log.push_back("ignored nonfinite or nonnumeric value: "+e.key);continue;}
        if(rule!=std::end(rules))next.*(rule->member)=clamp_value(e,rule->low,rule->high,log);
        else if(e.key=="c_pitch")next.control_power_per_pa.z=clamp_value(e,.0002,.0003,log);
        else if(e.key=="alpha_limit_deg")next.alpha_limit.y.fill(clamp_value(e,45.6,50,log)*3.141592653589793/180);
        else {
            const double scale=clamp_value(e,e.key=="roll_rate_scale"?.5:.8,e.key=="roll_rate_scale"?1.:1.2,log);
            if(e.key=="roll_rate_scale")for(auto& v:next.roll_limit.y)v*=scale;
            else if(e.key=="cd0_scale")for(auto& v:next.cd0.y)v*=scale;
            else for(auto& v:next.induced_k.y)v*=scale;
        }
    }
    if(!valid_parameters(next)){log.push_back("invalid parameters; defaults retained");return false;}
    parameters=next;if(dry)*dry=dry_next;return true;
}
inline bool load(const std::filesystem::path& path,Parameters& parameters,std::vector<std::string>& log,DryThrustSettings* dry=nullptr) {
    std::error_code error;
    if(!std::filesystem::exists(path,error)){log.push_back("file absent; candidate-2 defaults");return false;}
    if(std::filesystem::file_size(path,error)>65536||error){log.push_back("unreadable or oversized file; defaults retained");return false;}
    std::ifstream file(path,std::ios::binary);
    if(!file){log.push_back("cannot read file; defaults retained");return false;}
    const std::string text((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
    return apply(text,parameters,log,dry);
}
}
