// SPDX-License-Identifier: MIT
#pragma once
#include <cmath>
#include <cstddef>
#include <cstring>
#include <initializer_list>

namespace f23radar {
constexpr double stockRate = 1.1344640137963142; // 65 degrees/s
constexpr double candidateRate = 2.0 * stockRate;
constexpr double stockModelRate = 1.1344638839364052;
constexpr double stockNoise = -214.6256510640482;
constexpr double marginDb = 3.0;
// Native update compares elapsed time BEFORE processing and resets it to dt.
// At 60 Hz, 20.3 ms therefore processes only every other tick (4.33 deg travel).
// Keep the gate below one tick so 130 deg/s scans sample every 2.17 degrees.
constexpr double candidateInterval = 0.016;
constexpr std::size_t radarSize = 0x5e68;
constexpr std::size_t modelSize = 0x3078;

inline double get(const void* p, std::size_t offset) {
    double value;
    std::memcpy(&value, static_cast<const char*>(p) + offset, sizeof(value));
    return value;
}
inline void put(void* p, std::size_t offset, double value) {
    std::memcpy(static_cast<char*>(p) + offset, &value, sizeof(value));
}
inline bool same_value(double a, double b) { return std::abs(a - b) < 1e-8; }

// The callback runs only for the F-23B's identified primary APG-73 object.
// Native mode changes can restore stock rates, so apply absolute values each tick.
// No target, missile, mode-selection, scan-volume or memory-timer fields are written.
struct Tune {
    void* radar = nullptr;
    void* model = nullptr;
    double originalModelRate = 0;
    double originalInterval = 0;
    double appliedInterval = 0;
    bool active = false;

    int update(void* r, void* m, bool airSearch) {
        if (r != radar || m != model) {
            radar = r; model = m; active = false;
        }
        if (!airSearch) {
            if (active) {
                for (auto offset : {0x4360u, 0x4368u})
                    if (same_value(get(r, offset), candidateRate)) put(r, offset, stockRate);
                if (same_value(get(m, 0x190), candidateRate)) put(m, 0x190, originalModelRate);
                if (same_value(get(m, 0x278), appliedInterval)) put(m, 0x278, originalInterval);
                if (same_value(get(m, 0x1d0), stockNoise - marginDb)) put(m, 0x1d0, stockNoise);
                active = false;
            }
            return 0;
        }
        const double beam = get(m, 0x188);
        const double rate = get(m, 0x190);
        const double noise = get(m, 0x1d0);
        if (!std::isfinite(beam) || beam < 0.01 || beam > 0.1
            || (!same_value(rate, stockModelRate) && !same_value(rate, candidateRate))
            || (!same_value(noise, stockNoise) && !same_value(noise, stockNoise - marginDb))) {
            update(r, m, false); // Restore our own fields; preserve an external change.
            return -4;
        }
        if (!active) {
            if (!same_value(rate, stockModelRate) || !same_value(noise, stockNoise)) return -4;
            originalModelRate = rate;
            originalInterval = get(m, 0x278);
            if (!same_value(originalInterval, 1.6 * beam / rate)) return -4;
            active = true;
        }
        // Preserve native exceptional slew rates, including the 240-degree/s limit.
        for (auto offset : {0x4360u, 0x4368u}) {
            const double current = get(r, offset);
            if (same_value(current, stockRate) || same_value(current, candidateRate))
                put(r, offset, candidateRate);
        }
        appliedInterval = candidateInterval;
        put(m, 0x190, candidateRate);
        put(m, 0x278, appliedInterval);
        // The native detector subtracts this value before computing return probability.
        // Lowering it by 3 dB adds 3 dB of signal margin. It is not transmitter power.
        put(m, 0x1d0, stockNoise - marginDb);
        return 1;
    }
};
}
