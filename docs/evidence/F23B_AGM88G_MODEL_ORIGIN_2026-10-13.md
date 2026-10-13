# AGM-88G original visual source

`F23B_AGM88G.edm` is a project-original procedural mesh.
Its editable geometry comes from `tools/blender/build_agm88g_model.py`.
The generator follows the MALICE material setup and uses the official Blender EDM exporter.

The installed stock HARM supplies only two measurements: 13.90 ft overall bounding length and 10-inch body diameter.
The project multiplies these measurements by 0.75, giving a 10.43-ft body and 7.5-inch diameter.
No stock vertices, faces, normals, UV maps, materials, or textures were decoded or copied.

The project authors the pointed nose, recessed nozzle, markings, four mid-body wings, and four tail fins.
Wing span is 16.85 inches. Tail-fin span is 13.70 inches. These spans are explicit project choices for bay clearance.
Both sets are clocked 45 degrees. Their largest transverse width or height is 11.95 inches.
The export has 5,658 triangles and six project materials.

Model SHA-256: `0ca4fec728c1f8f4ba6251894bcbbd6d5cb2cd698c43f047ef94f328000532f3`.
The model metadata is `Mods/aircraft/F-23B/Shapes/F23B_AGM88G.json`.
The mesh uses **CC BY-SA 4.0**, with attribution to **F-23B project contributors**.
The generator uses its existing **MIT** file grant. Canonical release terms are in `LICENCE.md` and `CREDITS.md`.

All three weapon shape references use this original model. The stock loadout picture remains an installed-game reference.
Legacy smoke origins and both motor nozzle positions use the same 0.75 visual factor.
A 1,538-value comparison finds seven changed shape/origin values and no missile performance change.

The measured minimum closed-bay clearance is 2.55, 4.01, and 2.24 inches at stations 3, 4, and 6.
The unchanged doors clear all three stores through their 110-degree motion.
The existing trapezes provide 11.02 inches of travel. Swept-volume checks clear deployment and downward separation.
The project-HARM controller deploys before consent and retracts before door closure. Runtime deployment remains unverified.
Those numbers describe the current F-23B model and these project stores, rather than real weapon dimensions or performance.

Build with the installed Blender and official EDM addon:

```powershell
blender --background --factory-startup --python tools/blender/build_agm88g_model.py -- --addon-path PATH_TO_OFFICIAL_ADDON --stock-size docs/evidence/F23B_AGM88G_MODEL_ORIGIN_2026-10-13/stock-size.json --output OUTPUT/F23B_AGM88G.edm --mesh-output OUTPUT/F23B_AGM88G.mesh.json --blend-output OUTPUT/F23B_AGM88G.blend
```
