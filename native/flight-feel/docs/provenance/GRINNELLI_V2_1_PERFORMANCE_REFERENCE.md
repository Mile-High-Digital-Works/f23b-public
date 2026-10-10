# Grinnelli performance reference

The source is https://github.com/grinnellidesigns/f-22a at tag v2.1.0, commit
`24dc1f51a8d0d9427c7bd3c368ccabd3e0ade53c`. Individual pinned raw files were downloaded read-only
and SHA-256 hashed; no upstream clone or installed content was imported.
Exact hashes and derived-file records are in the repository
`config/licensing/third-party-code-reuse.json`.

RAPTOR.h and RAPTOR.cpp carry Branden Hooper's GPL-3.0-or-later notice.
The EFM directory's MIT notice does not replace those file-specific notices.
See root COPYING and THIRD_PARTY_NOTICES.md, included with corresponding source
in aircraft packages. No upstream binary or content asset is distributed.

The model adapts Mach-indexed aerodynamic/thrust schedules, nonlinear input
shaping, damping, soft G/AoA limiting, spool and burner timing, and trim.
September 4 modifications remove mass/inertia cancellation, separate the two
throttles, integrate trim with elapsed time (100 Hz gain calibration), preserve
signed gravity-corrected normal load, and use suspension contact for ground
scheduling. Hidden thrust vectoring remains disabled.

The reference tables describe an F-22 mod and are not measured YF-23 data.
The report's level-flight estimates solve lift/drag equations without solving
pitch-moment trim or integrating closed-loop flight. They are diagnostic
equation outputs, not demonstrated speeds or handling qualities.
