# F23B_AGM88G value origin — October 12, 2026

Project-defined simulation values, not real weapon performance. No other mod supplies a value.

## Installed source

DCS 2.9.30.28738, read during named flight `harm-1-2` through its existing GUI probe:

| Effective stock table | SHA-256 |
| --- | --- |
| `weapons_table.weapons.missiles.AGM_88` | `1795ea3130bfd360ab39d0d1398a1bbf5bd1002bfe2beb2b487c71b9c2a34669` |
| `rockets[21]`, identity `AGM_88`, wsType 4/4/8/65 | `c22ac2d2f6bb489ec7a6c397a4ce8d0091abd20bf5e35eafd4ce9cda2aade7a6` |
| Stock HARM loadout | `a187b58b17450c982219017c78b222f465e6fb15c5ea46b79156e7eb0c0dc0bd` |

Snapshots are in `docs/research/v15-build/additions/`. ED's `Scripts/Database/wsTypes.lua` also maps AGM_88 to 65.
Stock CLSID: `{B06DD79A-F21E-4EB9-BD9D-AB3844618C93}`.
The readable installed ChinaAssetPack `mils_ld10.lua`, lines 181–182, confirms that `wAmmunitionAntiRad` uses `anti_rad_missile`. No LD-10 numerical value was copied.

## Project factors

The definition starts with the captured stock rockets fields plus the effective server blocks.
Every numeric value not listed below has factor **1.0**. Identity strings, category/type identifiers,
shape registration and declaration bookkeeping are identifiers, not performance values.

| Field | Stock × project factor | Result |
| --- | --- | --- |
| M, mass, fm.mass | 361 × 0.75 | 270.75 kg |
| Diam; fm.caliber and fm.L | 254 / 0.254 × 0.75 | 190.5 mm / 0.1905 m |
| fm.S | 0.051 × 0.75² | 0.0286875 m² |
| fm.Ix/Iy/Iz | stock × mass factor × size factor² | stock × 0.421875 |
| boost.impulse | 235 × 1.25 | 293.75 |
| march.impulse | 206 × 1.25 | 257.5 |
| march.work_time; legacy t_marsh | 12.5 / 10 × 1.2 | 15 / 12 s |
| D_max; Range_max | 151000 / 134000 × 1.4 | 211400 / 187600 m |
| v_mid; Mach_max | 400 / 3.2 × 1.25 | 500 / 4.0 |
| Nr_max; autopilot.Areq_limit | 12 / 14 × 1.5 | 18 / 21 |
| RmaxData, RmaxData2, LoftData, LoftData2 coefficients | each stock coefficient × 1.4 | declared envelope expansion |
| Loadout Cx_pil | stock loadout 0.0011 × 0.75² | 0.00061875 |

Motor fuel masses, boost duration, starts, aerodynamic coefficient arrays, seeker, conversion,
emitter filters, warheads and proximity fuze retain factor 1.0. The native stock seeker excludes
`EWR`/`ewr`; no new emitter catalog or matching rule is introduced.
The direct bay loadout omits the external stock LAU-118 rail. The existing main-bay attachment
coordinates and downward ejection are retained. ED's `AGM-88` mesh is referenced unchanged.
The smaller simulated dimensions do **not** establish a smaller rendered mesh or full bay clearance.
No stock model or game file is copied, resized or modified.

## Integration and evidence

The helper aliases only the exact project resource identity, family 4/4/8, to stock HARM 65
for Hornet payload callers. World/AI retain the project identity. A/A alias families remain 4/4/7.
A/G uses the existing weapon-release control through the input wrapper: 0.20 s bay opening,
1.25 s bounded native consent, 0.25 s post-release hold and 0.20 s closing. These timings are
PROJECT CHOICES; the open/close rates and hold are the existing bay values.
Existing saved release assignments are read without rewriting any owner's control file.

RUNTIME PASS: stock `harm-1-2` and project `harm-2` each show HARM RDY/H-OFF, one launch,
open bay before release, automatic closure and a hit on the emitting search radar.
Both begin with the radar 30 miles ahead at 15,000 ft. Release occurs at about 23 miles after setup.
Measured peak sampled speed: stock 1,785 knots; project 2,522 knots in this geometry.
The project live descriptor confirms mass, dimensions, impulse and load values above. Its recorded
seeker, conversion, warhead and fuze tables equal the recorded stock tables.
The first stock run lacked sensor priority and did not launch; the second corrected the pilot sequence.
All flights lasted about 180 s, closed only their owned process and restored all 27 protected hashes.

NOT PROVEN: full rendered bay clearance/smaller mesh, launches from stations 4 and 6, PB/SP,
maximum range or load envelope, exact 30-mile release, multiplayer and VR.
See `docs/research/v15-build/additions/harm-result.json` for source hashes, runs and observations.

## October 13 visual update

The project-original model replaces the earlier stock appearance.
Its [model origin](F23B_AGM88G_MODEL_ORIGIN_2026-10-13.md) records size, authored geometry, exhaust scaling, and the licence.
The model is 10.43 ft long with a 7.5-inch body. All three stations clear by at least two inches.
The earlier smaller-mesh and bay-clearance gaps are closed. Missile performance values retain the October 12 factors.
