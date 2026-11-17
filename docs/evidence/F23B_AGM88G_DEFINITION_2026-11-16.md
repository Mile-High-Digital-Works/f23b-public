# Project AGM-88G definition

November 16, 2026. v1.5.2 source preparation.

The project HARM is an independent `anti_rad_missile` projectile with the `wAmmunitionAntiRad` class.
Its declaration supplies the flight model, motor, seeker, control and warhead settings without reading runtime weapon tables.
The helper uses native Hornet HARM data for cockpit queries. MALICE and Block II retain their existing project-data routing.
The projectile and AI retain the independent project identity.

## Retained settings

| Setting | Factor |
| --- | --- |
| Body dimensions and mass | 0.75 each |
| Motor impulse, `v_mid` and `Mach_max` | 1.25 |
| March duration | 1.2 |
| Guidance duration | 1.4 |
| Load limits | 1.5 |
| Lift slope | 1.64 |

The declared range limits, seeker detection distances, motor fuel, aerodynamic curves, control gains and warhead settings retain their existing values.
The loadout remains one project store without an external rail. No new numerical tuning is included.
The numerical baseline originated in DCS game tables, with project factors documented in the [value-origin record](https://github.com/Mile-High-Digital-Works/f23b-public/blob/4db5722da63212563b82767ead54c6573220c266/docs/evidence/F23B_AGM88G_VALUE_ORIGIN_2026-11-16.md).
This rewrite does not alter that provenance or the existing licence terms.

## Declaration changes and limits

`LaunchDistData`, `MinLaunchDistData`, `RmaxData`, `RminData`, `LoftData`, and each corresponding `2` variant are absent.
These are ten tables containing 1,174 entries. Every other declared weapon and loadout field matches the v1.5.1 definition exactly.
This changes the projectile's launch-data inputs. It does not establish identical AI launch decisions.
The simulation retains native anti-radiation guidance. No GPS navigation or millimeter-wave terminal seeker is added.

Offline registration and integration checks pass. Comparative flight validation, cockpit TOO behavior and AI launch decisions remain unverified.
PB/SP modes, an 85-mile TOO hit and automatic TOO loft remain unverified.
The [known limits](https://github.com/Mile-High-Digital-Works/f23b-public/blob/main/KNOWN_ISSUES.md) describe player-facing restrictions.

Source: `Mods/aircraft/F-23B/Weapons/F23B_AGM88G.lua`.
Source SHA-256: `412c1fbe842e5af906f46936ab10db5ffd8a14555d5d26b6c5da421c4ac42ab9`.
Reference v1.5.1 SHA-256: `fad0e9273e6a7d8b64797abb0c8d4b109cd2c4c12ee9c251cab6792b8889c04c`.
