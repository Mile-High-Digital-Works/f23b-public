# Missile visual status

November 16, 2026.

The project HARM uses an original AARGM-ER-inspired silhouette with four long strakes and compact tail controls.
A restrained gray body, contrasting radome and simple markings distinguish the weapon.
The MALICE and Block II models and their charcoal-gray finish are unchanged.

The HARM body remains 10.43 feet long and 7.5 inches in diameter.
Its exhaust position and longitudinal bounds are unchanged. The revised surfaces fit within the previous overall bounds.

The official Blender EDM exporter produced 5,658 triangles with six bound materials and no degenerate triangles.
The comparative render was inspected. Block II rebuilds were byte-identical before and after the generator change.
These checks do not establish DCS loading or bay clearance during release; runtime validation is pending.
No weapon performance values change with this visual update.

HARM model SHA-256: `27d7f565cefd777b4a6d93c2d0455f09021efa9ace6b3527701a6f00a02b181f`.
[Geometry and material record](F23B_MISSILE_VISUALS_2026-11-16/F23B_AGM88G.json).
The [October 13 record](F23B_MISSILE_VISUALS_2026-10-13.md) retains the earlier model hashes and production details.

Source: `tools/blender/build_agm88g_model.py`.
