![F-23B v1.4 in DCS](https://raw.githubusercontent.com/Mile-High-Digital-Works/f23b-public/v1.4/docs/images/f23b-exterior.jpg)

![Ten included liveries](https://raw.githubusercontent.com/Mile-High-Digital-Works/f23b-public/v1.4/docs/images/f23b-liveries.jpg)

![Afterburner at dusk](https://raw.githubusercontent.com/Mile-High-Digital-Works/f23b-public/v1.4/docs/images/f23b-afterburner.jpg)

Hi everyone, v1.4 is up. Quite a bit has changed since my September 19 post, so this brings everything together.

The aircraft now leaves the DCS game files alone. The weapon connection runs in memory while you fly the F-23B.
MALICE has loft, a second motor pulse, and long-range loft control. An owner flight recorded a 336-mile A-50 kill with continuing target support.
That is one supported flight, not a promised range for every shot.

- AI F-23Bs fire MALICE and Block II. Friendly aircraft no longer enter automatic target ranking, and SA now reaches 640 NM.
- Ground and water collisions work. Steering uses a Hornet-like radius and reduces with speed. The nose strut is stiffer under braking.
- The pilot view has one canopy frame. This release adds the new afterburner visuals and all ten liveries.
- The weapon and radar connection supports DCS 2.9.30.28738. There is a simpler setup and an illustrated pilot guide.

Requires an installed, activated F/A-18C Hornet. Single-player; VR and multiplayer not yet tested.

Two equal ways to install, both with the same aircraft, weapons, liveries and documentation:

| Method | Download from the current release |
| --- | --- |
| Copy and paste | [F-23B-v1.4.zip](https://github.com/Mile-High-Digital-Works/f23b-public/releases/latest) |
| Installer | [F-23B-v1.4-Setup.exe](https://github.com/Mile-High-Digital-Works/f23b-public/releases/latest), which does the same thing for you |

**Manual install**

- Close DCS.
- Extract the ZIP and drag its contents into `Saved Games\DCS`.

**Manual upgrade**

- Close DCS and delete `Mods\aircraft\F-23B` and `Mods\aircraft\F-23B-Player` first.
- Copy the extracted contents into the same profile.

**Manual removal**

- Close DCS and delete those two aircraft folders.
- Delete the supplied `F23B-...` folders in `Liveries\F-23B` and, optionally, `F-23B-docs`.

If you used September 17 or 18, run DCS Repair with **Check all files (slow)** once before your first manual upgrade.
Those versions changed stock game files; copying the ZIP cannot undo that. Setup can remove verified old changes automatically.
It asks for administrator permission only when that cleanup is needed.

Setup also accepts an unchanged manual install from a known release. If the files differ, it tells you what to back up first.
The ZIP layout works with OvGME and Open Mod Manager when their target is your Saved Games DCS profile.
Windows warns about the unsigned installer? Use the ZIP.

After a DCS update, both methods still install. If the weapon and radar connection is off, the aircraft says so in game.
Custom weapons, radar upgrades and SA 640 are then unavailable. Flight, the native cockpit and radar, bays, lights and liveries remain available.
Use a compatible release before firing the custom weapons.

Start an F-23B Quick Start mission to fly the Langley livery, or choose any supplied `F-23B |` scheme.
The [pilot guide](https://github.com/Mile-High-Digital-Works/f23b-public/blob/main/docs/PILOT_GUIDE.md) covers arming, bays, both weapons, SA and ground handling.
[Installation details](https://github.com/Mile-High-Digital-Works/f23b-public/blob/main/INSTALL.md) are included too.

Feedback is welcome here or in [GitHub Discussions](https://github.com/Mile-High-Digital-Works/f23b-public/discussions).
Please include the F-23B version and DCS version when reporting a problem.
