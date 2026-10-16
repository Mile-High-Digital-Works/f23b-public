// SPDX-License-Identifier: MIT
#pragma once
namespace f23b {
struct DryThrustSettings {
    bool enabled{true};
    // end_gain balances the recorded dry thrust against the October 15 corrected drag at Mach 2.0,
    // 35,000 ft (Stage 25). It was 1.4735572004347315 with the uncorrected drag.
    double start_mach{1.3},end_mach{2.0},end_gain{1.5506798378948843};
};
}
