// SPDX-License-Identifier: MIT
#include "RadarTune.h"
#include "NativeTarget.h"
#include <array>
#include <cassert>
#include <fstream>
#include <iostream>
using namespace f23radar;
int main(int argc, char** argv) {
    // A non-primary Registered interface must resolve the MovingObject base.
    std::array<unsigned char, 1024> image{};
    std::array<unsigned char, 256> object{};
    auto w32 = [&](size_t at, uint32_t value) { std::memcpy(image.data()+at,&value,4); };
    auto wp = [](void* at, uintptr_t value) { std::memcpy(at,&value,8); };
    const auto base = reinterpret_cast<uintptr_t>(image.data());
    wp(object.data()+64,base+64); wp(image.data()+56,base+128);
    w32(128,1); w32(132,64); w32(144,192); w32(148,128);
    w32(200,1); w32(204,224); w32(224,256);
    w32(256,320); w32(264,16); w32(268,UINT32_MAX);
    constexpr char name[] = ".?AVMovingObject@@";
    std::memcpy(image.data()+336,name,sizeof(name));
    auto readable = [&](const void* p,size_t n) {
        auto a=reinterpret_cast<uintptr_t>(p);
        auto o=reinterpret_cast<uintptr_t>(object.data());
        return (a>=base && a-base<=image.size() && n<=image.size()-(a-base))
            || (a>=o && a-o<=object.size() && n<=object.size()-(a-o));
    };
    assert(moving_object(object.data()+64,readable)==object.data()+16);
    w32(268,0); assert(!moving_object(object.data()+64,readable));
    w32(268,UINT32_MAX); w32(276,2); assert(!moving_object(object.data()+64,readable));
    w32(276,0); w32(200,129); assert(!moving_object(object.data()+64,readable));
    w32(200,1); image[336]='x'; assert(!moving_object(object.data()+64,readable));
    assert(!moving_object(nullptr,readable));
    std::array<unsigned char, radarSize> radar{};
    std::array<unsigned char, modelSize> model{};
    if (argc == 3) {
        std::ifstream r(argv[1], std::ios::binary), m(argv[2], std::ios::binary);
        assert(r.read(reinterpret_cast<char*>(radar.data()), radar.size()));
        assert(m.read(reinterpret_cast<char*>(model.data()), model.size()));
    } else {
        put(radar.data(), 0x4360, stockRate); put(radar.data(), 0x4368, stockRate);
        put(model.data(), 0x188, 0.028805892909860402);
        put(model.data(), 0x190, stockModelRate);
        put(model.data(), 0x1d0, stockNoise);
        put(model.data(), 0x278, 1.6 * get(model.data(), 0x188) / stockModelRate);
    }
    const auto originalRadar = radar;
    const auto originalModel = model;
    Tune tune;
    assert(tune.update(radar.data(), model.data(), false) == 0);
    assert(radar == originalRadar && model == originalModel);
    assert(tune.update(radar.data(), model.data(), true) == 1);
    assert(same_value(get(radar.data(), 0x4360), candidateRate));
    assert(same_value(get(radar.data(), 0x4368), candidateRate));
    assert(same_value(get(model.data(), 0x190), candidateRate));
    assert(same_value(get(model.data(), 0x278), candidateInterval));
    // Reproduce the native scheduler, including reset-to-dt after detection.
    auto sample_gap = [](double interval) {
        constexpr double dt = 1.0 / 60.0;
        double elapsed = dt, previous = 0, maxGap = 0;
        for (int tick = 1; tick <= 120; ++tick) {
            if (elapsed >= interval) {
                const double time = tick * dt;
                maxGap = std::fmax(maxGap, time - previous);
                previous = time; elapsed = dt;
            } else elapsed += dt;
        }
        return maxGap * candidateRate;
    };
    const double beamWidth = 2.0 * std::atan(get(model.data(), 0x188));
    assert(sample_gap(1.6 * get(model.data(), 0x188) / candidateRate) > beamWidth);
    assert(sample_gap(candidateInterval) < beamWidth);
    assert(same_value(get(model.data(), 0x1d0), stockNoise - 3));
    // Every byte outside the five authorized fields must remain unchanged.
    for (size_t i = 0; i < radar.size(); ++i)
        if (!(i >= 0x4360 && i < 0x4370)) assert(radar[i] == originalRadar[i]);
    for (size_t i = 0; i < model.size(); ++i)
        if (!((i >= 0x190 && i < 0x198) || (i >= 0x1d0 && i < 0x1d8)
            || (i >= 0x278 && i < 0x280))) assert(model[i] == originalModel[i]);
    const auto appliedRadar = radar; const auto appliedModel = model;
    for (int i = 0; i < 10000; ++i) assert(tune.update(radar.data(), model.data(), true) == 1);
    assert(radar == appliedRadar && model == appliedModel); // No accumulating gain.
    put(radar.data(), 0x4360, stockRate); // Native mode reset between callbacks.
    assert(tune.update(radar.data(), model.data(), true) == 1);
    assert(radar == appliedRadar);
    assert(tune.update(radar.data(), model.data(), false) == 0);
    assert(radar == originalRadar && model == originalModel);
    // A changed detector must reject the complete tune without a partial write.
    put(model.data(), 0x1d0, -100);
    auto incompatible = model;
    assert(tune.update(radar.data(), model.data(), true) == -4);
    assert(radar == originalRadar && model == incompatible);
    model = originalModel;
    assert(tune.update(radar.data(), model.data(), true) == 1);
    put(model.data(), 0x1d0, -100);
    assert(tune.update(radar.data(), model.data(), true) == -4);
    assert(radar == originalRadar && model == incompatible);
    model = originalModel;
    put(radar.data(), 0x4360, 4.1887902047863905); // Preserve exceptional native slew.
    assert(tune.update(radar.data(), model.data(), true) == 1);
    assert(get(radar.data(), 0x4360) == 4.1887902047863905);
    std::cout << "PASS: field scope, matching detector timing, idempotence, mode reset, restoration, rejection\n";
}
