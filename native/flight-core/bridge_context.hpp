// SPDX-License-Identifier: MIT
#pragma once
namespace f23b::flight_core {
// The retained public BodyState contract lacks pitch. The bridge captures this
// SDK argument directly, before evaluating the independent force model.
inline double sdk_pitch_rad{};
}
