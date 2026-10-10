// SPDX-License-Identifier: GPL-3.0-or-later
#include "f23b/performance/engine_mapping.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace perf = f23b::performance;

namespace {
int failures = 0;
void check(bool condition, const char* message) {
    if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
void near(double actual, double expected, double tolerance, const char* message) {
    if (!std::isfinite(actual) || std::abs(actual - expected) > tolerance) {
        ++failures;
        std::cerr << "FAIL: " << message << " actual=" << actual
                  << " expected=" << expected << '\n';
    }
}
}

int main() {
    perf::EngineOutput off{};
    check(perf::engine_parameter_value(perf::EngineParameterField::Combustion,
        off, 0.8) == 0.0, "off engine reports no combustion");
    check(perf::raw_nozzle_argument(off) == 0.0, "off engine closes raw nozzle channel");
    check(perf::burner_stage_argument(off) == 0.0, "off engine has no burner stage");

    perf::EngineOutput mil{};
    mil.combustion = true;
    mil.throttle_state = 1.025;
    mil.spool_fraction = 0.8255813953488372;
    mil.thrust_n = 154000.0;
    near(perf::engine_parameter_value(perf::EngineParameterField::RelatedRpm,
        mil, 0.8), 0.95, 1.0e-12, "dry MIL maps to 95 percent related RPM");
    check(perf::engine_parameter_value(perf::EngineParameterField::CoreRelatedThrust,
        mil, 0.8) <= 1.0 + 1.0e-12, "dry core-related thrust does not fake AB");

    perf::EngineOutput ab = mil;
    ab.throttle_state = 1.10;
    ab.spool_fraction = 1.0;
    ab.afterburner_fraction = 1.0;
    ab.thrust_n = 275000.0;
    near(perf::engine_parameter_value(perf::EngineParameterField::RelatedRpm,
        ab, 0.8), 1.10, 1.0e-12, "full burner maps to 110 percent related RPM");
    near(perf::raw_nozzle_argument(ab), 1.0, 1.0e-12, "full burner fully opens raw nozzle channel");
    near(perf::burner_stage_argument(ab), 0.97, 1.0e-12, "full burner reaches accepted staged-art top");
    check(perf::engine_parameter_value(perf::EngineParameterField::CoreRelatedThrust,
        ab, 0.8) > 1.0, "full burner publishes excess core-related thrust");

    if (failures) { return EXIT_FAILURE; }
    std::cout << "All performance engine-mapping tests passed\n";
    return EXIT_SUCCESS;
}
