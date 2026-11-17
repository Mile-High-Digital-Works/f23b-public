# Known limitations

The v1.5.2 source is awaiting DCS validation. The latest published download remains v1.5.1.

## Compatibility

- **Multiplayer is untested. VR is untested.**
- Windows and an installed, activated F/A-18C Hornet are required.
- The weapon and radar connection supports **DCS 2.9.31.29434**.
- On incompatible DCS builds, the connection switches off and displays an in-game message.
- Radar missile launches, including stock AIM-120C, are blocked while the connection is off.
- MALICE, Block II, project HARM operation, radar upgrades and SA 640 are also unavailable.
- Flight, the native cockpit and radar, bays, lights and liveries remain available.
- The installer is unsigned. Use the ZIP if Windows warns about it.

## Weapons and AI

- Hornet cockpit labels remain: AMRAAM selects MALICE; Sidewinder selects Block II; HARM selects the project AGM-88G.
- MALICE recovery after sustained tracking loss has offline checks but remains unconfirmed in DCS.
- Full missile engagement ranges are not established. Published design values do not guarantee a hit.
- MALICE loft control does not impose a hard altitude ceiling.
- Block II helmet-cued shots remain unverified. Post-launch datalink and lock-on after launch are not implemented.
- The revised HARM definition and cockpit data routing await comparative flight checks. AI launch decisions remain unverified.
- HARM PB and SP modes, an 85-mile TOO hit and automatic TOO loft remain unverified.
- AI MALICE can launch with closed main bay doors. AI Block II bay sequencing and AI HARM attacks remain unverified.
- AI missile enhancements require the weapon helper on the simulation controlling the missile.
- AI and dedicated-server missiles without that helper do not receive those enhancements.
- Move MALICE from station 3 to station 2 in older missions.

## Flight and controls

- Performance is a fictional aircraft design, not measured YF-23 performance.
- AI top speed, supersonic idle response and the complete regional damage response remain unverified.
- Ground steering authority decreases with speed. Use rudder and differential braking at higher speeds.
- Automatic migration of existing Hornet weapon bindings has offline checks; migration from older saved profiles remains unverified.

[Pilot guide](docs/PILOT_GUIDE.md) ·
[Feedback and questions](https://github.com/Mile-High-Digital-Works/f23b-public/discussions) ·
[Report a bug](https://github.com/Mile-High-Digital-Works/f23b-public/issues/new?template=bug_report.yml)
