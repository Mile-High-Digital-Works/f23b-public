// SPDX-License-Identifier: GPL-3.0-or-later
#include "steering.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace f23b::bridge;
void require(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
int main() {
    const double degree = 3.14159265358979323846 / 180.0;
    const double f23 = 5.651507 + 1.401347, hornet = 3.02 + 2.319;
    // Same turn radius as the Hornet at every native angle below the limit.
    for (double native : {1.0, 4.28, 10.0, 16.0, 45.0, 70.0}) {
        const double shown = nose_wheel_yaw(native * degree);
        require(std::abs(f23 / std::tan(shown) - hornet / std::tan(native * degree)) < 1e-9,
            "turn radius differs from the Hornet");
        require(std::abs(nose_wheel_yaw(-native * degree) + shown) < 1e-12, "left and right differ");
    }
    // Normal-gain full deflection: 16 degrees becomes about 20.75 degrees.
    require(std::abs(nose_wheel_yaw(16.0 * degree) / degree - 20.74631) < 1e-4, "low-gain limit");
    // The native 75-degree yaw limit still applies.
    require(nose_wheel_yaw(75.0 * degree) <= nose_yaw_limit_rad + 1e-12, "yaw limit exceeded");
    require(nose_wheel_yaw(-80.0 * degree) >= -nose_yaw_limit_rad - 1e-12, "negative yaw limit exceeded");
    require(nose_wheel_yaw(0.0) == 0.0, "centred wheel moved");
    require(std::isnan(nose_wheel_yaw(std::numeric_limits<double>::quiet_NaN())), "NaN hidden");
    // Taxi turns keep the full angle: full low-gain pedal up to 7 m/s, and parked.
    for (double speed : {0.0, 3.0, 4.3, 7.0})
        require(nose_wheel_yaw(16.0 * degree, speed) == nose_wheel_yaw(16.0 * degree), "taxi angle limited");
    // At speed, the commanded turn stays within 0.3 g, and left and right match.
    for (double speed : {10.0, 18.0, 26.0, 60.0}) {
        for (double native : {6.1, 12.22, 16.0}) {
            const double yaw = nose_wheel_yaw(native * degree, speed);
            require(yaw > 0 && yaw <= nose_wheel_yaw(native * degree), "limit changed sign or grew");
            require(speed * speed * std::tan(yaw) / f23 <= 0.3 * 9.80665 + 1e-9, "lateral limit exceeded");
            require(nose_wheel_yaw(-native * degree, speed) == -yaw, "limited left and right differ");
        }
    }
    // The failed September 29 stop: half pedal at 26 m/s commanded 8 degrees; now about 1.76.
    require(std::abs(nose_wheel_yaw(6.1 * degree, 26.0) / degree - 1.7581) < 1e-3, "26 m/s limit");
    require(std::abs(nose_wheel_yaw(6.1 * degree, -26.0) - nose_wheel_yaw(6.1 * degree, 26.0)) < 1e-15,
        "reverse speed limited differently");
    const double nan = std::numeric_limits<double>::quiet_NaN();
    require(nose_wheel_yaw(16.0 * degree, nan) == nose_wheel_yaw(16.0 * degree), "unknown speed changed angle");
    require(nose_wheel_yaw(0.0, 26.0) == 0.0, "centred wheel moved at speed");
}
