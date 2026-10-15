# F-23B v1.5 — pilot guide

AI F-23Bs use the player's engine multipliers; AI top speed in DCS is unverified.
Idle thrust now falls above Mach 1.3 to slow the jet; flight confirmation is pending.

[Download the one-page printable guide](https://github.com/Mile-High-Digital-Works/f23b-public/releases/latest/download/F-23B-v1.5-Pilot-Guide.html).
Open it in a browser and select Print.

![The Hornet cockpit used by the F-23B](images/f23b-cockpit.jpg)

| Step | Action |
| --- | --- |
| Arm | Use your Hornet controls. Set MASTER ARM to ARM and select A/A. |
| Select | Select AMRAAM for MALICE, or Sidewinder for Block II. The Hornet display retains those labels. |
| Fire | Press the trigger. The correct bay opens, the weapon fires, then the bay closes. A brief press queues the sequence. |
| MALICE | Designate a valid radar or supported network target. Maintain target updates through midcourse. The second pulse starts at seeker search, except within about 10 seconds of the target. |
| Block II | Enable IR cooling and obtain a seeker lock before firing. With a designated hostile aircraft, the seeker locks only that aircraft; undesignate to let it choose. It does not lock other air-to-air missiles. Native helmet indications are available; a helmet-cued shot and post-launch datalink are not proven. |
| HARM | Load AGM-88G AARGM-ER (F-23B project-defined) on station 2, 3 or 7, or select the SEAD preset. MASTER ARM: ARM; A/G; STORES; HARM; TOO. Assign the HARM DDI with sensor-control left/right, select the emitter, then Cage/Uncage for H-OFF. Use your existing Weapon Release button; a brief press opens the bay, lowers the trapezes, releases once and closes it. Each HARM needs its own Cage/Uncage handoff. |
| Fuel | Higher applied thrust consumes extra fuel. The internal 10,800 lb lasts as if the jet carried 18,000 lb. Afterburner still burns about 1,200 lb per minute. Unlimited-fuel missions retain unlimited fuel. |
| Damage | Wing damage reduces lift and roll authority, adds drag and rolls toward the damaged side. Fin damage reduces yaw authority. A missing whole wing cannot be held level. |
| SA scale | Open SA on a DDI. Press SCL to cycle through the available scales, including 640 NM (about 737 miles). |
| Ground | Use small steering inputs. Steering authority reduces with speed. Use rudder and differential braking at higher speeds. |

An earlier MALICE build recorded a 336-mile A-50 kill with continuing support. Motor C has a 58.7-mile fixture hit; its long shot remains unverified. Range depends on launch height, speed, target motion and support.
MALICE, project HARM and Block II share a subdued charcoal-grey finish. Stock DCS missiles retain their normal colours. The new finish and original Block II visual await DCS confirmation.
Project HARM retains the native cockpit label and uses its own compact model: 10.43 ft long with a 7.5-inch body. Three fit the roof trapezes, stations 2/3/7. The trapezes lower the project HARMs before release. Each bay door carries one MALICE (stations 4/6) and one Block II; a third MALICE fits station 2. The doors open 150 degrees, so the door missiles swing clear of the HARMs. TOO guidance and hits are recorded from all three HARM stations. The bay adds about 0.6 s between the press and the launch, in any view and master mode, including Pullback. TOO flies straight to the radar and does not loft; recorded TOO hits are 23 miles from 15,000 ft and 68 miles from 35,000 ft at Mach 1.2. For a longer TOO shot, pull the nose up before release and hold it through the bay delay; keep the radar inside the 30-degree TOO field or the handoff drops. Declared range and TOO detection are the stock HARM values. PB and SP are unverified.

A level release at 35,000 ft and Mach 1.2 hit an emitter 67.93 miles away in 214.80 seconds. Peak speed was 3,327 knots. Speed fell below 600 knots after 51.87 miles flown. An 85-mile hit and faster turning remain unverified. TOO handoff was lost before the 45-degree turn-test release.

The bay sequence also operates the gun door. Do not bypass it with donor trigger mappings.
If **weapon and radar connection OFF** appears, get a compatible release before using custom weapons or SA 640.

Single-player; VR and multiplayer not yet tested.
[Installation](../INSTALL.md) · [Feedback](https://github.com/Mile-High-Digital-Works/f23b-public/discussions)
