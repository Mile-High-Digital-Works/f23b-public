# F-23B Black Widow II

A fictional stealth fighter for DCS World, with the native Hornet cockpit, radar, datalink and SA page.
v1.5 adds a new flight core, HARM capability, revised internal bays and improved missile support.
v1.5.1 updates the weapon and radar connection for DCS 2.9.31.29434.

![F-23B in DCS World](docs/images/f23b-exterior.jpg)

## Download v1.5.1

Requires Windows and an installed, activated **DCS: F/A-18C Hornet**.
The weapon and radar connection supports **DCS 2.9.31.29434**.
**Multiplayer is untested. VR is untested.**

| Method | Download |
| --- | --- |
| Copy and paste | [F-23B-v1.5.1.zip](https://github.com/Mile-High-Digital-Works/f23b-public/releases/download/v1.5.1/F-23B-v1.5.1.zip) |
| Installer | [F-23B-v1.5.1-Setup.exe](https://github.com/Mile-High-Digital-Works/f23b-public/releases/download/v1.5.1/F-23B-v1.5.1-Setup.exe) |

Both options install identical aircraft, weapons, ten liveries and documentation.
No separate Python installation is needed. Current installations change no DCS game files.

For the ZIP, close DCS and extract its contents into your active Saved Games profile.
For setup, close DCS, run the EXE, confirm the game and profile folders, then select **Install / Repair**.
Read [installation and upgrade instructions](INSTALL.md) before replacing an older version.
Start an **F-23B Quick Start** mission and use your Hornet controls.

[Illustrated pilot guide](docs/PILOT_GUIDE.md) · [What's new](docs/RELEASE_NOTES.md) · [Known limits](KNOWN_ISSUES.md)

## Aircraft and weapons

- A project flight core with stable handling, smoother hard pulls and low-speed recovery.
- Reduced pitch bounce and strong high-altitude acceleration.
- Design speeds of about Mach 2.0 dry and Mach 2.47 in afterburner at 35,000 ft.
- Thrust-based fuel use with improved endurance.
- Native Hornet radar, MSI, datalink, EW and RWR, plus a **640 NM** SA scale.
- Sequenced internal bays that open before each player weapon release.
- **AIM-424 MALICE:** radar-guided air-to-air missile with loft control, two motor pulses and retained target support.
- **AIM-9X Block II:** infrared air-to-air missile with flare rejection.
- **AGM-88G AARGM-ER:** project HARM with native HARM controls and TOO guidance.
- A SEAD preset with three HARMs, two MALICE and two Block II missiles.
- Wing and fin damage effects, speed-sensitive ground steering, exterior lights and afterburner visuals.
- A smoother exterior skin, charcoal-grey project missiles and ten included liveries.

![The Hornet cockpit used by the F-23B](docs/images/f23b-cockpit.jpg)

![Ten included liveries](docs/images/f23b-liveries.jpg)

## Flying and compatibility

Use **AMRAAM** on the Hornet display to select MALICE, and **Sidewinder** to select Block II.
Press the trigger for air-to-air missiles. Use **Weapon Release** for HARM.
A brief press queues the bay opening, release and closing sequence.

Keep the radar operating while supporting MALICE.
Radar OFF, standby, inhibit or loss of the radar connection ends retained support.
Missile range depends on altitude, speed, target motion and support; maximum engagement ranges are not established.

After a DCS update, the weapon and radar connection can switch off.
If **weapon and radar connection OFF** appears, MALICE, Block II, radar upgrades and SA 640 are unavailable.
Flight, the native cockpit and radar, bays, lights and liveries remain available.
Use a [release compatible with your DCS build](https://github.com/Mile-High-Digital-Works/f23b-public/releases).

AI MALICE can launch while the main bay doors remain closed.
AI missile enhancements require the helper on the simulation controlling the missile.
See [known limitations](KNOWN_ISSUES.md) for multiplayer, VR and weapon restrictions.

## Help and licences

Choose an **F-23B |** scheme in the mission editor or rearming menu.
If Windows warns about the unsigned installer, use the ZIP.

[Feedback and questions](https://github.com/Mile-High-Digital-Works/f23b-public/discussions) ·
[Report a bug](https://github.com/Mile-High-Digital-Works/f23b-public/issues/new?template=bug_report.yml)

Software uses **GPL-3.0-or-later**, with retained file-level MIT grants.
Eligible original documentation and creative content use **CC BY-SA 4.0**.
Purchased models and derived textures retain separate asset terms.

Credits: **F-23B project contributors** for the flight core and integration; **SytaPastel** for the licensed YF-23 visual source.

[Licence and asset terms](LICENCE.md) · [Credits and component notices](CREDITS.md) ·
[Build and test](BUILDING.md) · [Source layout](docs/SOURCE_LAYOUT.md) · [Contributing](CONTRIBUTING.md)

The release includes corresponding software source and SHA-256 checksums.

THIS MATERIAL IS NOT MADE OR SUPPORTED BY EAGLE DYNAMICS SA.
