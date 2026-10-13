# SPDX-License-Identifier: MIT
"""Build project-original HARM and Block II visuals with the MALICE EDM route.

Only stock bounding length and descriptor diameter inform this mesh. Every
vertex, fin planform, material and marking below is authored by this project.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys

import bpy
from mathutils import Vector

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--addon-path', required=True)
parser.add_argument('--stock-size', required=True)
parser.add_argument('--output', required=True)
parser.add_argument('--mesh-output', required=True)
parser.add_argument('--blend-output', required=True)
parser.add_argument('--blockii', action='store_true', help='Build the original Block II visual instead of HARM')
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
# Same official addon setup as the accepted MALICE builder. This generator
# stays self-contained so its corresponding source can ship with the mesh.
addon = Path(args.addon_path).resolve()
sys.path.insert(0, str(addon))
sys.path.insert(0, str(addon.parent))
import Materials
sys.modules['materials'] = Materials
import addon_utils
addon_utils.modules_refresh()
enabled = addon_utils.enable('io_scene_edm', default_set=True, persistent=True)
if enabled is None:
    assert 'FINISHED' in bpy.ops.preferences.addon_enable(module='io_scene_edm')
from math_tools import ROOT_TRANSFORM_MATRIX
from materials.materials import build_material_descriptions

stock = json.loads(Path(args.stock_size).read_text(encoding='utf-8'))
assert stock['geometry_decoded'] is False
SCALE = 1.0 if args.blockii else .75
SHAPE = 'F23B_AIM9X_BLOCKII' if args.blockii else 'F23B_AGM88G'
PREFIX = 'BLOCKII' if args.blockii else 'AGM88G'
LENGTH = stock['length_m'] * SCALE
DIAMETER = stock['diameter_m'] * SCALE
RADIUS = DIAMETER / 2
# The running modern stock motor uses nozzle_position.x = -2.1.
TAIL = -1.6 if args.blockii else -2.1 * SCALE
NOSE = TAIL + LENGTH
WING_RADIUS = .105 if args.blockii else .214
FIN_RADIUS = .135 if args.blockii else .174
inverse = ROOT_TRANSFORM_MATRIX.inverted()
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)


def material(name, color):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    m.node_tree.nodes.clear()
    shader = m.node_tree.nodes.new('EdmDefaultShaderNodeType')
    shader.post_init(build_material_descriptions()['EDM_Default_Material'])
    shader.inputs['Base Color'].default_value = (*color, 1)
    output = m.node_tree.nodes.new('ShaderNodeOutputMaterial')
    m.node_tree.links.new(shader.outputs[0], output.inputs['Surface'])
    return m


body = material(SHAPE + '_body', (.10, .125, .15))
radome = material(SHAPE + '_radome', (.045, .055, .068))
metal = material(SHAPE + '_metal', (.075, .09, .105))
dark = material(SHAPE + '_nozzle', (.018, .023, .028))
ink = material(SHAPE + '_stencil', (.25, .28, .30))
yellow = material(SHAPE + '_band', (.24, .20, .105))


def mesh(name, vertices, faces, mat, smooth=False):
    name = name.replace('AGM88G', PREFIX)
    data = bpy.data.meshes.new(name)
    data.from_pydata([inverse @ Vector(v) for v in vertices], [], faces)
    data.update()
    obj = bpy.data.objects.new(name, data)
    bpy.context.collection.objects.link(obj)
    data.materials.append(mat)
    data.uv_layers.new(name='UVMap')
    for polygon in data.polygons:
        polygon.use_smooth = smooth
    return obj


def lathe(name, rings, mat, caps=True, reverse=False):
    count = 64
    vertices, sections, faces = [], [], []
    for x, radius in rings:
        section = []
        for i in range(1 if radius == 0 else count):
            section.append(len(vertices))
            angle = i * 2 * math.pi / count
            vertices.append((x, radius * math.cos(angle), radius * math.sin(angle)))
        sections.append(section)
    for a, b in zip(sections, sections[1:]):
        for i in range(count):
            j = (i + 1) % count
            if len(b) == 1:
                faces.append((a[i], a[j], b[0]))
            elif len(a) == 1:
                faces.append((a[0], b[j], b[i]))
            else:
                faces.append((a[i], a[j], b[j], b[i]))
    if caps:
        if len(sections[0]) > 1:
            faces.append(tuple(reversed(sections[0])))
        if len(sections[-1]) > 1:
            faces.append(tuple(sections[-1]))
    if reverse:
        faces = [tuple(reversed(face)) for face in faces]
    return mesh(name, vertices, faces, mat, True)


body_factor = RADIUS / (.1905 / 2) if args.blockii else 1.0
lathe('AGM88G_body', [(TAIL, .073 * body_factor), (TAIL + .04, .081 * body_factor),
                     (TAIL + .13, RADIUS - .0006),
                     (NOSE - .53, RADIUS - .0006)], body, caps=False)
# An original pointed ogive and eight swept surfaces give a HARM-like layout.
NOSE_LENGTH = .10 if args.blockii else .53
nose_rings = ([(NOSE - .53, RADIUS - .0006)] if args.blockii else []) + [(NOSE - NOSE_LENGTH, RADIUS - .0006)]
lathe('AGM88G_radome', nose_rings +
      [(NOSE - NOSE_LENGTH + NOSE_LENGTH * i / 24,
        (RADIUS - .0006) * math.cos(i / 24 * math.pi / 2))
       for i in range(1, 24)] + [(NOSE, 0)], radome)
lathe('AGM88G_exit_lip', [(TAIL, .073 * body_factor), (TAIL, .057 * body_factor),
                        (TAIL + .022, .054 * body_factor)], metal, caps=False, reverse=True)
lathe('AGM88G_nozzle_wall', [(TAIL + .022, .054 * body_factor),
                           (TAIL + .10, .035 * body_factor)], dark, caps=False, reverse=True)
lathe('AGM88G_nozzle_throat', [(TAIL + .099, .035 * body_factor),
                             (TAIL + .101, .035 * body_factor)], dark)
for label, x, width, mat in [('warhead_band', NOSE - .64, .025, yellow),
                           ('fore_joint', NOSE - .54, .004, metal),
                           ('mid_joint', .15, .004, metal),
                           ('aft_joint', TAIL + .20, .004, metal)]:
    lathe('AGM88G_' + label, [(x, RADIUS), (x + width, RADIUS)], mat, caps=False)


def fin(name, angle, outline):
    center = Vector((sum(x for x, r in outline) / 4,
                     sum(r for x, r in outline) / 4))
    inset = [tuple(Vector(p).lerp(center, .12)) for p in outline]
    vertices = []
    for thickness, ring in [(-.0007, outline), (-.003, inset),
                            (.0007, outline), (.003, inset)]:
        for x, radius in ring:
            vertices.append((x, radius * math.cos(angle) - thickness * math.sin(angle),
                             radius * math.sin(angle) + thickness * math.cos(angle)))
    faces = [(7, 6, 5, 4), (12, 13, 14, 15)]
    for i in range(4):
        j = (i + 1) % 4
        faces += [(i, 4 + i, 4 + j, j), (8 + i, 8 + j, 12 + j, 12 + i),
                  (i, j, 8 + j, 8 + i)]
    return mesh(name, vertices, faces, body)


for k in range(4):
    angle = math.pi / 4 + k * math.pi / 2
    if args.blockii:
        # Project-authored short foreplanes and swept tails; no ED geometry.
        fin('AGM88G_mid_wing_' + str(k), angle,
            [(NOSE - .67, .057), (NOSE - .38, .057),
             (NOSE - .42, WING_RADIUS), (NOSE - .62, WING_RADIUS)])
        fin('AGM88G_tail_fin_' + str(k), angle,
            [(TAIL + .045, .051), (TAIL + .52, .061),
             (TAIL + .24, FIN_RADIUS), (TAIL + .075, FIN_RADIUS)])
    else:
        fin('AGM88G_mid_wing_' + str(k), angle,
            [(-.63, .088), (.34, .088), (-.05, WING_RADIUS), (-.49, WING_RADIUS)])
        fin('AGM88G_tail_fin_' + str(k), angle,
            [(TAIL + .045, .084), (TAIL + .62, .092),
             (TAIL + .33, FIN_RADIUS), (TAIL + .07, FIN_RADIUS)])


def stencil(text, x, angle, size, direction):
    bpy.ops.object.text_add()
    obj = bpy.context.object
    obj.name = PREFIX + '_label_' + text
    obj.data.body = text
    obj.data.size = size
    obj.data.resolution_u = 3
    obj.data.materials.append(ink)
    bpy.ops.object.convert(target='MESH')
    width = max(v.co.x for v in obj.data.vertices)
    radius = RADIUS - .0002
    for v in obj.data.vertices:
        tx, ty, _ = v.co
        a = angle - direction * ty / radius
        axial = x + (tx if direction == 1 else width - tx)
        v.co = inverse @ Vector((axial, radius * math.cos(a), radius * math.sin(a)))
    obj.data.update()
    obj.data.uv_layers.new(name='UVMap')


for angle, direction in [(math.pi / 2, 1), (3 * math.pi / 2, -1)]:
    stencil('AIM-9X BLOCK II' if args.blockii else 'AGM-88G', .10 if args.blockii else .46,
            angle, .021 if args.blockii else .032, direction)
    stencil('F-23B PROJECT', .10 if args.blockii else .46,
            angle + direction * .27, .012 if args.blockii else .017, direction)

bpy.context.view_layer.update()
quality = {'degenerate_triangles': 0, 'closed_solids': {}}
vertices, faces = [], []
for obj in bpy.context.scene.objects:
    if obj.type != 'MESH':
        continue
    data = obj.data
    data.calc_loop_triangles()
    points = [ROOT_TRANSFORM_MATRIX @ obj.matrix_world @ v.co for v in data.vertices]
    volume, edges = 0, {}
    for p in data.polygons:
        for a, b in zip(p.vertices, list(p.vertices[1:]) + [p.vertices[0]]):
            key = tuple(sorted((a, b)))
            edges[key] = edges.get(key, 0) + 1
    for tri in data.loop_triangles:
        a, b, c = (points[i] for i in tri.vertices)
        quality['degenerate_triangles'] += int((b - a).cross(c - a).length < 1e-13)
        volume += a.dot(b.cross(c)) / 6
    if any(token in obj.name for token in ('mid_wing', 'tail_fin', 'radome')):
        assert all(n == 2 for n in edges.values()), (obj.name, 'open solid')
        assert volume > 0, (obj.name, 'inward winding')
        quality['closed_solids'][obj.name] = {'outward_winding': True, 'volume_m3': volume}
    offset = len(vertices)
    vertices += [list(v) for v in points]
    faces += [[offset + i for i in p.vertices] for p in data.polygons]
assert quality['degenerate_triangles'] == 0, quality
bounds = [[min(v[i] for v in vertices), max(v[i] for v in vertices)] for i in range(3)]
assert abs(bounds[0][1] - bounds[0][0] - LENGTH) < 1e-5
output = Path(args.output).resolve()
output.parent.mkdir(parents=True, exist_ok=True)
blend = Path(args.blend_output).resolve()
blend.parent.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=str(blend))
result = bpy.ops.edm.export(filepath=str(output))
assert 'FINISHED' in result and output.stat().st_size > 1000, result
Path(args.mesh_output).write_text(json.dumps({'vertices': vertices, 'faces': faces}), encoding='utf-8')
report = dict(status='OFFLINE_PASS', visual_scale=SCALE, body_length_m=LENGTH,
              body_diameter_m=DIAMETER, mid_wing_span_m=2 * WING_RADIUS,
              tail_fin_span_m=2 * FIN_RADIUS, dcs_bounds_m=bounds,
              exhaust_x_m=TAIL, sha256=hashlib.sha256(output.read_bytes()).hexdigest(),
              bytes=output.stat().st_size, mesh_quality=quality,
              stock_model_sha256=stock['sha256'], stock_geometry_decoded=False,
              source='tools/blender/build_agm88g_model.py',
              shape=SHAPE, finish='Project charcoal grey; subdued identification band and grey stencils; visual only',
              provenance='Project-original procedural mesh; stock metadata gives overall length and body diameter only.',
              licence='CC-BY-SA-4.0; attribution: F-23B project contributors',
              simulation_performance_changed=False)
output.with_suffix('.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
print('MODEL_RESULT', json.dumps(report), flush=True)
