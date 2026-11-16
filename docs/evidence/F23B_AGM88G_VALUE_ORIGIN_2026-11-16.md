# F23B_AGM88G value origin: November 16, 2026

This record gives the factors of the released AGM-88G definition.
It supersedes the factor table in the [October 12 record](F23B_AGM88G_VALUE_ORIGIN_2026-10-12.md).
Project-defined simulation values, not real weapon performance. No other mod supplies a value.

## Baselines

The October 12 record identifies each baseline table by its SHA-256.

## Factors in the released definition

`Mods/aircraft/F-23B/Weapons/F23B_AGM88G.lua` applies these factors in v1.5 and v1.5.1.
They differ from the October 12 factor table as follows.

| Field | October 12 record | Released definition |
| --- | --- | --- |
| `D_max`, `Range_max` | baseline × 1.4 | baseline |
| `RmaxData`, `RmaxData2`, `LoftData`, `LoftData2` | baseline × 1.4 | baseline |
| `fm.Cya`, `fm.Cza` | baseline | baseline × 1.64 |
| `seeker.op_time`, `autopilot.op_time`, `seeker.pos_memory_time` | baseline | baseline × 1.4 |
| `X_back`, `Y_back`, `Z_back`, nozzle positions | baseline | baseline × 0.75 |

The other factors in the October 12 table do not change: mass 0.75, size 0.75, motor impulse 1.25,
march time 1.2, `v_mid` and `Mach_max` 1.25, load limits 1.5, and the loadout `Cx_pil`.
The declared range and the launch-zone tables keep their baseline values, so that AI, cockpit cues
and the TOO page behave as with the Hornet's own HARM. The nozzle positions follow the smaller body;
the motor values do not change.
