// SPDX-License-Identifier: GPL-3.0-or-later
#include <f23b/performance/performance_model.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>
using namespace f23b::performance;

// Close the loop around the real force model, including actuator slew and
// differential elevons. This isolated roll plant is not a six-DOF DCS claim.
int main() {
    int failures = 0, cases = 0;
    for (double ias : {100., 200., 330., 370., 450.})
    for (double density : {0.4, 1.225})
    for (double ix : {15000., 20215., 31100., 45000., 65000.})
    for (double dt : {0.001, 0.003, 0.006, 0.012, 0.02, 0.05})
    for (double command : {0., 0.04, 0.0499, 0.0501, 0.06, -0.06, 0.5, -1., 1.}) {
        Model model;
        Atmosphere air{density, 325., 4500.};
        MassState mass;
        mass.inertia_kg_m2.x = ix;
        mass.center_of_mass_m = {-0.65, -0.32, 0.};
        BodyState body;
        body.velocity_body_m_s.x = ias / std::sqrt(density / 1.225);
        body.angular_rate_rad_s.x = 0.1;
        PilotInput input;
        input.engine_left_running = input.engine_right_running = false;
        double sum = 0., squares = 0., peak_after_release = 0.;
        int samples = 0;
        bool finite = true;
        for (int n = 0; n < static_cast<int>(12. / dt); ++n) {
            const double t = n * dt;
            input.roll = t < 6. ? command : 0.;
            const auto output = model.step(dt, air, mass, body, input);
            body.angular_rate_rad_s.x += output.moment_n_m.x / ix * dt;
            body.roll_rad += body.angular_rate_rad_s.x * dt;
            finite &= std::isfinite(body.angular_rate_rad_s.x);
            if (t > 4. && t < 6.) {
                sum += body.angular_rate_rad_s.x;
                squares += body.angular_rate_rad_s.x * body.angular_rate_rad_s.x;
                ++samples;
            }
            if (t > 10.) peak_after_release = std::max(peak_after_release,
                std::abs(body.angular_rate_rad_s.x));
        }
        const double deviation = std::sqrt(std::max(0., squares / samples
            - (sum / samples) * (sum / samples)));
        ++cases;
        const bool follows_pilot = command == 0.
            || (sum / samples * command > 0. && std::abs(sum / samples) > 0.0001);
        if (!finite || !follows_pilot || deviation > 0.001 || peak_after_release > 0.001) {
            ++failures;
            if (failures < 20) std::cerr << "Unsettled roll: " << ias << ',' << density
                << ',' << ix << ',' << dt << ',' << command << " std=" << deviation
                << " release=" << peak_after_release << '\n';
        }
    }
    std::cout << cases << " closed-loop roll cases; " << failures << " failures\n";
    return failures ? 1 : 0;
}
