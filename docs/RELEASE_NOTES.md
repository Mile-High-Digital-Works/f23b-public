# F-23B Black Widow II v1.5.2

Release preparation is in progress. DCS validation and package checks are pending.
The [latest published release](https://github.com/Mile-High-Digital-Works/f23b-public/releases/latest) remains the available download.

## Prepared changes

- Keep radar launches blocked when the weapon connection is unavailable, with a specific message.
- Pair weapon-release inputs across master-mode changes.
- Disable tuning-file and recorder hooks in release builds; retain an explicit development build option.
- Reserve Lua stack space for the radar helper's status results.
- Align module versions, update the bay description, and correct livery country restrictions.
- Improve installer recovery, dedicated-server detection, source checks and automated test coverage.

The HARM visual uses long body strakes, compact tail controls and a gray finish.
The HARM definition is self-contained and retains its physical, motor, seeker, control and warhead settings.
Native Hornet data supplies cockpit HARM calculations. The projectile omits its previous launch-data tables.
Comparative flight behavior and AI launch decisions remain unverified.
The [definition record](evidence/F23B_AGM88G_DEFINITION_2026-11-16.md) gives the source identity and limits.
No changed runtime behavior is claimed as flight-validated here.

## Known limits

- DCS validation of the v1.5.2 maintenance build is pending.
- Multiplayer is untested. VR is untested.
- The weapon and radar connection supports DCS 2.9.31.29434.
- With the weapon and radar connection off, radar missile launches including stock AIM-120C are blocked; MALICE, Block II, project HARM operation, radar upgrades and SA 640 are unavailable.
- Full missile engagement ranges are not established.
- MALICE recovery after sustained tracking loss remains unconfirmed in DCS.
- Block II helmet-cued shots remain unverified; post-launch datalink is not implemented.
- The revised HARM definition and cockpit data routing await comparative flight checks. AI launch decisions remain unverified.
- HARM PB/SP modes, an 85-mile TOO hit and automatic TOO loft remain unverified.
- AI MALICE can launch with closed main bay doors; AI Block II bay sequencing and AI HARM attacks remain unverified.
- AI and dedicated-server missiles without the helper do not receive its missile enhancements.
- AI top speed, supersonic idle response and the complete regional damage response remain unverified.

Both installation methods must pass the same file-inventory and checksum checks before release.
Software source and checksums will accompany the completed release.

THIS MATERIAL IS NOT MADE OR SUPPORTED BY EAGLE DYNAMICS SA.
