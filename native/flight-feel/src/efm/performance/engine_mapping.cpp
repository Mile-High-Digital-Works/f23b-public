// SPDX-License-Identifier: GPL-3.0-or-later
#include "f23b/performance/engine_mapping.hpp"
#include "f23b/performance/grinnelli_v21_reference.hpp"

#include <algorithm>
#include <cmath>

namespace f23b::performance {
namespace ref = grinnelli_v21;
namespace {

[[nodiscard]] double bounded(double value, double low, double high) noexcept {
    if (!std::isfinite(value)) { return low; }
    return std::clamp(value, low, high);
}

[[nodiscard]] double dry_related_rpm(const EngineOutput& engine) noexcept {
    if (!engine.combustion) { return 0.0; }
    const double t = bounded(engine.throttle_state, ref::throttle_idle, ref::throttle_max);
    if (t <= ref::throttle_mil) {
        const double f = (t - ref::throttle_idle) / (ref::throttle_mil - ref::throttle_idle);
        return 0.67 + 0.28 * bounded(f, 0.0, 1.0);
    }
    return 0.95;
}

[[nodiscard]] double related_rpm(const EngineOutput& engine) noexcept {
    return bounded(dry_related_rpm(engine)
        + 0.15 * bounded(engine.afterburner_fraction, 0.0, 1.0), 0.0, 1.10);
}

[[nodiscard]] double dry_thrust_reference(double mach) noexcept {
    // The upstream dry table deliberately falls to zero above M1.8 while its
    // afterburning table remains strong. A cockpit-relative channel cannot use
    // zero as a divisor, so retain at least the reference static dry rating.
    return std::max(ref::static_maximum_dry_thrust_per_engine_n,
        interpolate(ref::mach.data(), ref::maximum_dry_thrust_per_engine_n.data(),
            ref::mach.size(), mach));
}

[[nodiscard]] double thrust_ratio(const EngineOutput& engine, double mach) noexcept {
    if (!engine.combustion || !std::isfinite(engine.thrust_n) || engine.thrust_n <= 0.0) {
        return 0.0;
    }
    return bounded(engine.thrust_n / dry_thrust_reference(mach), 0.0, 2.0);
}

} // namespace

double engine_parameter_value(EngineParameterField field,
    const EngineOutput& engine, double mach, double starter_progress) noexcept {
    const double starter = bounded(starter_progress, 0.0, 1.0);
    const double fan_related = related_rpm(engine);
    const double ratio = thrust_ratio(engine, mach);
    const double ab = engine.combustion
        ? bounded(engine.afterburner_fraction, 0.0, 1.0) : 0.0;
    switch (field) {
    case EngineParameterField::Rpm:
        return fan_related * 10'000.0;
    case EngineParameterField::RelatedRpm:
        return fan_related;
    case EngineParameterField::CoreRpm:
        return std::max(fan_related, starter * 0.22) * 13'000.0;
    case EngineParameterField::CoreRelatedRpm:
        return std::max(fan_related, starter * 0.22);
    case EngineParameterField::StarterRelatedRpm:
        return starter;
    case EngineParameterField::Thrust:
        return std::isfinite(engine.thrust_n) ? engine.thrust_n : 0.0;
    case EngineParameterField::RelatedThrust:
        return std::max(0.0, ratio);
    case EngineParameterField::CoreThrust:
        return std::max(0.0, std::min(engine.thrust_n, dry_thrust_reference(mach)));
    case EngineParameterField::CoreRelatedThrust:
        // Keep the dry channel at or below 1.0 and provide a continuous burner
        // excess for full-fidelity turbofan consumers.
        return std::min(std::max(0.0, ratio), 1.0) + ab;
    case EngineParameterField::FuelFlow:
        return std::isfinite(engine.fuel_flow_kg_s) ? engine.fuel_flow_kg_s : 0.0;
    case EngineParameterField::Combustion:
        return engine.combustion ? 1.0 : 0.0;
    case EngineParameterField::Temperature:
        return engine.combustion ? 320.0 + 650.0 * bounded(engine.spool_fraction, 0.0, 1.0) : 288.0;
    case EngineParameterField::Gain:
        return engine.combustion ? 1.0 : 0.0;
    case EngineParameterField::FlowSpeed:
        if (!engine.combustion) { return 0.0; }
        return 200.0 + 250.0 * bounded(engine.spool_fraction, 0.0, 1.0)
            + 450.0 * ab;
    }
    return 0.0;
}

double raw_nozzle_argument(const EngineOutput& engine) noexcept {
    if (!engine.combustion) { return 0.0; }
    const double spool = bounded(engine.spool_fraction, 0.0, 1.0);
    const double ab = bounded(engine.afterburner_fraction, 0.0, 1.0);
    return bounded(0.5 * spool + 0.5 * ab, 0.0, 1.0);
}

double burner_stage_argument(const EngineOutput& engine) noexcept {
    if (!engine.combustion) { return 0.0; }
    return 0.97 * bounded(engine.afterburner_fraction, 0.0, 1.0);
}

} // namespace f23b::performance
