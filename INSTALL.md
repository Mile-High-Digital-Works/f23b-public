# Install or remove F-23B v1.5.1

Requires Windows and an installed, activated **DCS: F/A-18C Hornet**.
The weapon and radar connection supports **DCS 2.9.31.29434**.
Both options install the same aircraft, weapons, ten liveries and documentation.
**Multiplayer is untested. VR is untested.**

| Method | Download |
| --- | --- |
| Copy and paste | [F-23B-v1.5.1.zip](https://github.com/Mile-High-Digital-Works/f23b-public/releases/download/v1.5.1/F-23B-v1.5.1.zip) |
| Installer | [F-23B-v1.5.1-Setup.exe](https://github.com/Mile-High-Digital-Works/f23b-public/releases/download/v1.5.1/F-23B-v1.5.1-Setup.exe), which does the same thing for you |

## One-time step for September 17 or 18 users

Those releases changed DCS game files. Copying the ZIP cannot undo those changes.
Before your first manual upgrade, close DCS and run **Repair** from the DCS launcher.
Select **Check all files (slow)** and let it finish, including the installed Hornet module.
Then follow **Upgrade** below. Do not restore the old mod's game-file backups afterward.

The changes were in three stock files: `aim120_family.lua`, `aim9_family.lua`, and the Hornet's `device_init.lua`.
A full repair restores those files. The old backup folder does not execute and can remain unused.
This conclusion follows the old installer code and [ED's repair documentation](https://www.digitalcombatsimulator.com/en/support/faq/709/).
A separate cleanup is unnecessary for these three stock-file changes.
Alternatively, use this release's setup to remove verified old changes automatically.

## Copy and paste

Extract the ZIP first. Its root contains `Mods`, `Liveries`, and `F-23B-docs`.
Use the Saved Games profile DCS actually uses, such as `Saved Games\DCS` or `Saved Games\DCS.openbeta`.
Do not copy the outer ZIP folder. This profile-relative layout also works with OvGME and Open Mod Manager.
Set the mod manager's target to that Saved Games profile, and disable an older package before enabling this one.

**Install**

- Close DCS.
- Drag the extracted contents into `Saved Games\DCS`.

**Upgrade**

- Close DCS and delete `Mods\aircraft\F-23B` and `Mods\aircraft\F-23B-Player` first.
- Copy the extracted contents into the same profile.

**Remove**

- Close DCS and delete those two aircraft folders.
- Delete the supplied `F23B-01-...` through `F23B-10-...` folders in `Liveries\F-23B`.
- Delete `F-23B-docs` if you no longer need it.

Keep any personal files before deleting an aircraft folder. Other aircraft and unrelated liveries can stay.
Licences, notices and corresponding-source details are together in `F-23B-docs`.

## Installer

1. Close DCS, its updater and ModelViewer.
2. Run **F-23B-v1.5.1-Setup.exe**.
3. Confirm the DCS game folder and Saved Games profile.
4. Select **Install / Repair**.

No separate Python installation is needed. Administrator permission is requested only to remove earlier F-23B game-file changes.
Use **Remove** in setup to remove the aircraft, supplied liveries and documentation.
Setup keeps your controls, missions and unrelated liveries.
If setup finds a changed supplied file, move it to a backup location before continuing.
Run **Install / Repair** again to restore the supplied file.

## Switching methods

Setup adopts an unchanged manual install when its complete file set matches a known release.
Known earlier releases are September 29, October 1, October 3, v1.4, and v1.5. The current package is also recognized.
If files differ, setup preserves them and displays one instruction:

> Move the existing F-23B aircraft, supplied livery and F-23B-docs folders to a backup location, then run setup again.

To switch from setup to manual installation, use setup's **Remove**, then follow **Install** above.
Setup keeps its receipt outside the DCS profile, under `%LOCALAPPDATA%\F-23B\installs`.
Development directory links are not supported by setup.

## DCS updates

Both methods allow installation on other DCS builds. The ZIP has no installation-time version check.
The aircraft checks the connection in game and shows **weapon and radar connection OFF** when it cannot connect.
MALICE, Block II, radar upgrades and the 640 NM SA scale are then unavailable.
Flight, the native Hornet cockpit and radar, bays, lights and liveries remain available.
Get a compatible build from [the latest release](https://github.com/Mile-High-Digital-Works/f23b-public/releases/latest).
DCS 2.9.29.27468 users can retain the [September 29 release](https://github.com/Mile-High-Digital-Works/f23b-public/releases/tag/release-2026-09-29).

## First flight and help

Start DCS and select an **F-23B Quick Start** mission. It uses the Langley livery.
Move MALICE from station 3 to station 2 in older missions.
Your Hornet controls apply. Read the [pilot guide](https://github.com/Mile-High-Digital-Works/f23b-public/blob/main/docs/PILOT_GUIDE.md).
Choose any supplied `F-23B |` livery in the mission editor or rearming menu.

**Windows warns about the unsigned installer:** use the ZIP.

[Feedback and questions](https://github.com/Mile-High-Digital-Works/f23b-public/discussions) ·
[Report a bug](https://github.com/Mile-High-Digital-Works/f23b-public/issues/new?template=bug_report.yml)

THIS MATERIAL IS NOT MADE OR SUPPORTED BY EAGLE DYNAMICS SA.
