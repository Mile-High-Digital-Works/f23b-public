# F-23B v1.5 — pilot guide

Requires an installed, activated **DCS: F/A-18C Hornet**.
The weapon and radar connection supports **DCS 2.9.30.28738**.
**Multiplayer is untested. VR is untested.**

[Download the printable guide](https://github.com/Mile-High-Digital-Works/f23b-public/releases/download/v1.5/F-23B-v1.5-Pilot-Guide.html).
Open it in a browser and select Print.

![The Hornet cockpit used by the F-23B](images/f23b-cockpit.jpg)

## Controls and bays

Use your Hornet controls. Existing Weapon Release and weapon-select bindings are copied once to the F-23B controls, with a backup.
A brief button press queues the bay opening, weapon release and bay closing.
Allow the sequence to finish before firing again.

| Weapon | Cockpit selection | Release control |
| --- | --- | --- |
| AIM-424 MALICE | A/A, AMRAAM | Trigger |
| AIM-9X Block II | A/A, Sidewinder | Trigger |
| AGM-88G AARGM-ER | A/G, HARM | Weapon Release |

The gun door also uses the bay sequence. Keep the supplied F-23B control mappings.

## MALICE

1. Set MASTER ARM to ARM.
2. Select A/A and AMRAAM.
3. Designate a valid radar or supported network target.
4. Press the trigger.
5. Keep the radar operating through impact.

MALICE uses loft guidance and a second motor pulse.
It rejects chaff and retains updates for a previously supported target after its radar track disappears.
Radar OFF, standby, inhibit or loss of the radar connection ends retained support.
Range depends on launch altitude, speed, target motion and support.

## Block II

1. Set MASTER ARM to ARM.
2. Select A/A and Sidewinder.
3. Enable IR cooling.
4. Obtain a seeker lock.
5. Press the trigger.

With a designated hostile aircraft, the seeker locks only that aircraft.
Undesignate to let the seeker select another target.
Block II rejects flares and does not select other air-to-air missiles.
Helmet indications are available; helmet-cued shots remain unverified.
Post-launch datalink and lock-on after launch are not implemented.

## HARM TOO

1. Load project AGM-88G on station 2, 3 or 7, or select the SEAD preset.
2. Set MASTER ARM to ARM.
3. Select A/G, STORES, HARM and TOO.
4. Assign the HARM DDI with sensor-control left or right.
5. Select the emitter.
6. Press Cage/Uncage for H-OFF.
7. Press Weapon Release.

Each HARM needs its own Cage/Uncage handoff.
The bay adds about 0.6 seconds before release.
Keep the emitter within the TOO field of view during that delay.
PB and SP modes remain unverified. Do not assume an automatic TOO loft.

The SEAD preset carries three HARMs, two MALICE and two Block II missiles.
MALICE uses stations 4 and 6 on the doors; station 2 accepts a third MALICE.
Move MALICE from station 3 to station 2 in older missions.

## Flying

Fuel use rises with applied thrust. Afterburner consumes fuel much faster than dry power.
The endurance setting makes 10,800 lb of internal fuel last as if the jet carried 18,000 lb.
Unlimited-fuel missions retain unlimited fuel.

Wing damage reduces lift and roll control. Fin damage reduces yaw control.
Use small steering inputs on the ground. Steering authority decreases with speed.
Use rudder and differential braking at higher speeds.

Open the SA page and press SCL to select **640 NM**, about 737 miles.
Choose an **F-23B |** livery in the mission editor or rearming menu.

## Connection and limits

If **weapon and radar connection OFF** appears, install a release compatible with your DCS version.
MALICE, Block II, radar upgrades and SA 640 are unavailable while the connection is off.
Flight, the native cockpit and radar, bays, lights and liveries remain available.

AI MALICE can launch with closed main bay doors.
AI missile enhancements require the helper on the simulation controlling the missile.
MALICE recovery after sustained tracking loss and maximum missile engagement ranges remain unverified.

[Installation](../INSTALL.md) · [Known limits](../KNOWN_ISSUES.md) ·
[Feedback](https://github.com/Mile-High-Digital-Works/f23b-public/discussions)
