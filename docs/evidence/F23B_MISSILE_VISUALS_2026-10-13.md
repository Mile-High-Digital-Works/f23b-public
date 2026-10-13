# Project-original missile visuals

MALICE, project AGM-88G and Block II use the same subdued charcoal-grey finish.
Dark nose sections, muted identification bands and grey stencils complete the design.
This is a fictional project appearance. It does not change radar signature or simulation performance.
Stock DCS missile shapes and game files remain unchanged.

All three meshes, materials and markings are original F-23B project work, under CC BY-SA 4.0.
Credit F-23B project contributors. The generators carry MIT notices.
No Eagle Dynamics geometry or texture is included.
Block II uses only ED's overall model length and stock descriptor diameter as size references.
Its illustrative foreplanes, nose and tail fins are project-authored.

| Visual | Body length | Body diameter | SHA-256 |
| --- | --- | --- | --- |
| MALICE | 10.11 ft | 10.04 in | 28e7fc067a793ab2650a5d415c63d09ff8f3693318fbcd75c11c29266e9bdb1f |
| Project HARM | 10.43 ft | 7.5 in | f4f9b91626576cf979c87335ed4cd020f71a5f04137a9e9d0e3c073b5d94ec9d |
| Block II | 9.93 ft | 5 in | 40013ada7670434f91f8ba4c951348a0cea78066d8b03839aade4f5b643b2ee3 |

MALICE and HARM geometry, UVs, normals, indices, transforms and material bindings are unchanged.
Block II has a 10.63-inch opposing tail-fin span. Its exhaust origin matches the existing weapon definition.
Stock loadout pictures and weapon flight values are retained.
The official Blender EDM exporter produced all three meshes without a degenerate triangle.
Blender previews were inspected. Native DCS loading and appearance of this finish remain OPEN.

Source: `tools/blender/build_malice_model.py` and `tools/blender/build_agm88g_model.py`.
The second generator selects Block II with `--blockii`.
Metadata and size references are in [the adjacent folder](F23B_MISSILE_VISUALS_2026-10-13/).
Use the installed official `io_scene_edm` Blender addon with `--addon-path`.
`tools/blender/render_project_missiles.py` renders original Blender outputs with their recorded base colours.
