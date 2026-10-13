# F-23B v1.5

AI F-23Bs use the player's engine multipliers; AI top speed in DCS is unverified.
Idle thrust now falls above Mach 1.3 to slow the jet; flight confirmation is pending.

This build is prepared for the final owner flight. It has not been published.

The default core provides carefree protection, a calmer roll response, smoother
hard pulls and low-speed recovery. All fourteen offline corners pass.
Dry power targets Mach 2.0 at 35,000 ft; afterburner targets remain unchanged.
The Hornet retains its cockpit, radar, datalink, engines, tanks and gear.
Fuel consumption now follows applied thrust; the dry/afterburner check was within 0.4% of target. Unlimited fuel is respected.
Eighteen regional contact shells replace the single airframe shell. Wheels and
ground lines retain their verified geometry. Wing and fin damage now reduce the corresponding lift and control authority. A named left outer-wing hit reached the core and produced a rolling moment toward the damaged side. Other regions remain unflown.

AGM-88G AARGM-ER is a fictional project HARM for main-bay stations 3, 4 and 6.
It uses stock HARM appearance and seeker behavior with explicit project performance factors.
The existing A/G Weapon Release control opens the bay, sends one release and closes it.
Stock and project HARM each recorded one TOO-guided emitter hit from station 3.
Stations 4/6, PB/SP, maximum envelope and full rendered bay clearance remain unverified.

MALICE retains motor C and its 58.7-mile fixture result. Its long shot remains open.
Block II does not lock other air-to-air missiles. With a designated hostile aircraft it locks only that aircraft.
MALICE holds its second motor pulse within about 10 seconds of the target, and accepts target updates again after a long gap.
These helper changes have three fixture shots; wider flight confirmation is pending. The selected revised slate appears in both builds.
AI MALICE may launch with closed main bay doors. No AI bay controller is added.
Block II post-launch datalink is not implemented. Multiplayer and VR remain unverified.

The original project flight core and SDK adapter replace the retired model in
both the DLL and source archive. Existing GPL and MIT file grants remain.
The ZIP and setup install identical files. No DCS game file is changed.

Read [installation](../INSTALL.md), [known limits](../KNOWN_ISSUES.md),
[licence](../LICENCE.md) and [credits](../CREDITS.md).

THIS MATERIAL IS NOT MADE OR SUPPORTED BY EAGLE DYNAMICS SA.
