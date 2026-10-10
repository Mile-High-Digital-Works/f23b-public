// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Hornet-facing engine/instrument mappings for the F-23B performance EFM.
 * Copyright (C) 2026 F-23B project contributors.
 */
#pragma once

#include "f23b/performance/performance_model.hpp"

namespace f23b::performance {

enum class EngineParameterField {
    Rpm,
    RelatedRpm,
    CoreRpm,
    CoreRelatedRpm,
    StarterRelatedRpm,
    Thrust,
    RelatedThrust,
    CoreThrust,
    CoreRelatedThrust,
    FuelFlow,
    Combustion,
    Temperature,
    Gain,
    FlowSpeed,
};

[[nodiscard]] double engine_parameter_value(EngineParameterField field,
    const EngineOutput& engine, double mach, double starter_progress = 0.0) noexcept;

[[nodiscard]] double raw_nozzle_argument(const EngineOutput& engine) noexcept;
[[nodiscard]] double burner_stage_argument(const EngineOutput& engine) noexcept;

} // namespace f23b::performance
