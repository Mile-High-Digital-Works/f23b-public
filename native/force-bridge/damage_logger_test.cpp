// SPDX-License-Identifier: GPL-3.0-or-later
#define WIN32_LEAN_AND_MEAN
#include "damage_logger.hpp"
#include <cassert>
#include <iostream>
#include <string>
namespace { int calls=0,cell=0;double value=0;
void original(int element,double integrity){++calls;cell=element;value=integrity;}
void check(int n,int e,double i){assert(calls==n && cell==e && value==i);}
}
int main(int argc,char** argv) {
    assert(argc==2);const auto log=std::filesystem::absolute(argv[1]);
    SetEnvironmentVariableW(L"F23B_FORCE_BRIDGE_DAMAGE",nullptr);
    f23b::bridge::damage::emit_and_forward(2.5,35,.625,&original);check(1,35,.625);
    assert(!std::filesystem::exists(log));
    SetEnvironmentVariableW(L"F23B_FORCE_BRIDGE_DAMAGE",log.c_str());
    f23b::bridge::damage::emit_and_forward(3.5,23,.3125,&original);check(2,23,.3125);
    std::ifstream input(log);std::string line;std::getline(input,line);assert(line=="3.5,23,0.3125");
    std::string extra;assert(!std::getline(input,extra));input.close();
    const auto invalid=log/L"missing"/L"log.csv";
    SetEnvironmentVariableW(L"F23B_FORCE_BRIDGE_DAMAGE",invalid.c_str());
    f23b::bridge::damage::emit_and_forward(4.5,99,.875,&original);check(3,99,.875);
    const std::wstring oversized(3000,L'x');
    SetEnvironmentVariableW(L"F23B_FORCE_BRIDGE_DAMAGE",oversized.c_str());
    f23b::bridge::damage::emit_and_forward(5.5,42,.125,&original);check(4,42,.125);
    SetEnvironmentVariableW(L"F23B_FORCE_BRIDGE_DAMAGE",nullptr);
    std::cout<<"PASS: disabled, enabled, unwritable and oversized log path; native receives all four unchanged calls\n";
}
