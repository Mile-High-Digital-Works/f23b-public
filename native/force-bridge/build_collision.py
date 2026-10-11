"""Build private contact geometry from the accepted exterior, without saving it.

Run in Blender with the accepted BLEND open. The wheel paths and tire radii are
measured; the fuselage uses a convex contact hull, not a detailed damage model.
Airframe collision lines along the measured skin give ground and water contact.
The aircraft Damage table must declare the four shells and every line name.
The production Hornet damage table cannot be paired with this reduced hull.
"""
import argparse
import hashlib
import json
import sys
sys.dont_write_bytecode = True
from pathlib import Path

import bpy
import bmesh
from mathutils import Vector
from mathutils.bvhtree import BVHTree

parser = argparse.ArgumentParser()
parser.add_argument('--addon', required=True)
parser.add_argument('--output', required=True)
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
source = Path(bpy.data.filepath)
source_hash = hashlib.sha256(source.read_bytes()).hexdigest().upper()
if source_hash != 'BB46D4DA2CEDBF1CEEDE884134EF092F3FB024236CEA5AD8DB3D03223EA244B1':
    raise RuntimeError('Expected accepted intake-final.blend')
output = Path(args.output)
if output.exists() or output.with_suffix('.json').exists():
    raise FileExistsError(output)
sys.path.insert(0, 'C:\\Users\\nicho\\Documents\\F23B-Production\\f23b\\tools\\blender')
from remap_f23b_hornet_visuals import enable_edm_exporter, action_fcurves
enable_edm_exporter(Path(args.addon))

legs = [('F', 'NOSE', 0), ('L', 'PORT_MAIN', 5), ('R', 'STARBOARD_MAIN', 3)]
scene = bpy.context.scene
scene.frame_set(100)
gear_roots = {'D1_GEAR_ROOT_' + n for _, n, _ in legs}
for obj in scene.objects:
    if obj.name not in gear_roots and obj.animation_data and obj.animation_data.action:
        obj.animation_data.action = None
paths = {key: [] for key, _, _ in legs}
transforms = {key: [] for key, _, _ in legs}
for frame in range(100, 201):
    scene.frame_set(frame)
    bpy.context.view_layer.update()
    for key, name, _ in legs:
        obj = bpy.data.objects['D2_' + name + '_WHEEL_ROTATION_ROOT']
        paths[key].append(list(obj.matrix_world.translation))
        transforms[key].append(bpy.data.objects['D1_GEAR_ROOT_' + name].matrix_world.copy())
radii = {}
centers = {}
for key, name, _ in legs:
    root = bpy.data.objects['D2_' + name + '_WHEEL_ROTATION_ROOT']
    center = root.matrix_world.translation
    centers[key] = transforms[key][-1].inverted() @ center
    for matrix, point in zip(transforms[key], paths[key]):
        if (matrix @ centers[key] - Vector(point)).length > 1e-5:
            raise RuntimeError('Rotation hierarchy does not reproduce measured wheel path')
    points = [o.matrix_world @ v.co for o in root.children if o.type == 'MESH' for v in o.data.vertices]
    radii[key] = max(((p.x-center.x)**2 + (p.z-center.z)**2)**.5 for p in points)
    if not .24 < radii[key] < .32:
        raise RuntimeError(f'Unexpected wheel radius {key}: {radii[key]}')
fuselage = bpy.data.objects['YF23_Fuselage']
hull_points = [fuselage.matrix_world @ v.co for v in fuselage.data.vertices]


# Airframe collision lines. DCS finds airframe contact with ground and water
# through collision lines; the wheel-only set let a gear-up F-23B pass through
# the sea on September 28. Each line follows the accepted exterior skin and uses
# an ED Damage.lua cell name that has a matching aircraft Damage entry.
def points_of(*names):
    return [bpy.data.objects[n].matrix_world @ v.co for n in names for v in bpy.data.objects[n].data.vertices]


def surface(*names):
    vertices, faces = [], []
    for name in names:
        obj = bpy.data.objects[name]
        base = len(vertices)
        vertices += [obj.matrix_world @ v.co for v in obj.data.vertices]
        faces += [[base + i for i in poly.vertices] for poly in obj.data.polygons]
    return BVHTree.FromPolygons(vertices, faces)


def stations(first, last, step=0.25):
    count = max(2, int(abs(last - first) / step) + 1)
    return [first + (last - first) * i / (count - 1) for i in range(count)]


def envelope(points, lower):
    # Only the convex envelope can meet flat ground or water. It also bridges
    # bay openings where a ray passes into the airframe.
    hull = []
    for p in sorted(points, key=lambda p: p.x):
        while len(hull) >= 2:
            a, b = hull[-2], hull[-1]
            cross = (b.x - a.x) * (p.z - a.z) - (b.z - a.z) * (p.x - a.x)
            if (cross <= 0) if lower else (cross >= 0):
                hull.pop()
            else:
                break
        hull.append(p)
    return hull


def skin(tree, xs, y, lower):
    direction = Vector((0.0, 0.0, 1.0 if lower else -1.0))
    hits = [tree.ray_cast(Vector((x, y, -20.0 if lower else 20.0)), direction)[0] for x in xs]
    line = envelope([h.copy() for h in hits if h is not None], lower)
    if len(line) < 2:
        raise RuntimeError(f'Collision line at y={y} has fewer than two skin points')
    return line


# Closed weapons, cannon and nose-gear doors complete the belly. Main-gear doors are open at rest.
underside = surface('YF23_Fuselage', 'YF23_BombBay_1', 'YF23_BombBay_2', 'YF23_CanonBay', 'YF23_LandingBay_1')
canopy_names = ('YF23_CanopyFrame_1', 'YF23_CanopyFrame_2', 'YF23_CanopyTint_1', 'YF23_CanopyTint_2')
topside = surface('YF23_Fuselage', *canopy_names)
tail_left, tail_right = points_of('YF23_Elevator_1'), points_of('YF23_Elevator_2')
nose_x = max(p.x for p in hull_points)
aft_x = min(p.x for p in hull_points)
span = max(abs(p.y) for p in hull_points)


def tail_lines(points):
    top = max(p.z for p in points)
    tip = sorted((p.copy() for p in points if p.z > top - 0.12), key=lambda p: p.x)
    low = min(p.z for p in points)
    leading = []
    for i in range(7):
        z = low + (top - low) * i / 6
        band = [p for p in points if abs(p.z - z) <= 0.12]
        if band:
            leading.append(max(band, key=lambda p: p.x).copy())
    return [tip[0], tip[-1]], leading


fin_l_top, fin_l_centre = tail_lines(tail_left)
fin_r_top, fin_r_centre = tail_lines(tail_right)
nose_tip = max(hull_points, key=lambda p: p.x).copy()
airframe_lines = {
    'NOSE_CENTER': envelope([nose_tip] + skin(underside, stations(nose_x - 0.05, 8.6), 0.0, True), True),
    'NOSE_BOTTOM': skin(underside, stations(8.6, 5.0), 0.0, True),
    'FUSELAGE_BOTTOM': skin(underside, stations(5.0, -5.5), 0.0, True),
    'TAIL_BOTTOM': skin(underside, stations(-5.5, aft_x + 0.05), 0.0, True),
    'FUSELAGE_TOP': skin(topside, stations(4.2, aft_x + 0.05), 0.0, False),
    'COCKPIT': skin(topside, stations(8.1, 4.2), 0.0, False),
    'ENGINE_L': skin(underside, stations(1.0, -6.0), 1.6, True),
    'ENGINE_R': skin(underside, stations(1.0, -6.0), -1.6, True),
    'FIN_L_TOP': fin_l_top, 'FIN_L_CENTER': fin_l_centre,
    'FIN_R_TOP': fin_r_top, 'FIN_R_CENTER': fin_r_centre,
}
for side, sign in (('L', 1.0), ('R', -1.0)):
    for cell, y0 in (('IN', 3.4), ('CENTER', 4.8), ('OUT', span - 0.12)):
        airframe_lines['WING_' + side + '_' + cell] = skin(underside, stations(4.0, -6.0, 0.1), sign * y0, True)

# Retain source data before clearing the scene. These are project hull approximations.
sys.path.insert(0, str(Path(__file__).resolve().parent))
from contact_geometry import hull, region_parts, packed
body_poly = hull(hull_points)
extra_polys = [hull(points_of(*canopy_names)), hull(points_of('YF23_BombBay_1', 'YF23_BombBay_2', 'YF23_CanonBay', 'YF23_LandingBay_1'))]
fin_polys = {'L': hull(tail_left), 'R': hull(tail_right)}
regions, partition_receipt = region_parts(body_poly, extra_polys, airframe_lines, fin_polys, span)
geometry_receipt = {k: [packed(p) for p in pieces] for k, pieces in regions.items()}
source_polys = {'body': packed(body_poly), 'extra': [packed(p) for p in extra_polys], 'fins': {k: packed(v) for k,v in fin_polys.items()}}
bpy.ops.wm.read_factory_settings(use_empty=True)
enable_edm_exporter(Path(args.addon))
root = bpy.data.objects.new('Scene Root', None)
bpy.context.scene.collection.objects.link(root)
for key, name, arg in legs:
    control = bpy.data.objects.new('GEAR_' + key, None)
    bpy.context.scene.collection.objects.link(control)
    control.parent = root
    control.location = transforms[key][0].translation
    control.rotation_mode = 'QUATERNION'
    control.animation_data_create()
    control.animation_data.action = bpy.data.actions.new(str(arg) + '_F23_GEAR_CONTACT')
    previous = None
    down_inverse = transforms[key][-1].to_quaternion().inverted()
    down_offset = Vector(paths[key][-1]) - transforms[key][-1].translation
    for i, matrix in enumerate(transforms[key]):
        q = matrix.to_quaternion() @ down_inverse
        if (q @ down_offset + control.location - Vector(paths[key][i])).length > 1e-5:
            raise RuntimeError('Normalized pivot changed the measured path')
        if previous is not None and q.dot(previous) < 0:
            q.negate()
        control.rotation_quaternion = q
        control.keyframe_insert(data_path='rotation_quaternion', frame=100+i)
        previous = q.copy()
    for curve in action_fcurves(control.animation_data.action):
        for k in curve.keyframe_points:
            k.interpolation = 'LINEAR'
    compression = bpy.data.objects.new('F23_STRUT_' + key, None)
    bpy.context.scene.collection.objects.link(compression)
    compression.parent = control
    compression.animation_data_create()
    compression.animation_data.action = bpy.data.actions.new(str(arg+1) + '_F23_STRUT_COMPRESSION')
    for frame, travel in [(100, 0.0), (200, 0.12)]:
        compression.location = (0, 0, travel)
        compression.keyframe_insert(data_path='location', frame=frame)
    for curve in action_fcurves(compression.animation_data.action):
        for k in curve.keyframe_points:
            k.interpolation = 'LINEAR'
    bpy.ops.mesh.primitive_uv_sphere_add(segments=24, ring_count=12, radius=radii[key])
    shell = bpy.context.object
    shell.name = 'WHEEL_' + key
    shell.parent = compression
    shell.location = down_offset
    shell.EDMProps.SPECIAL_TYPE = 'COLLISION_SHELL'
    mesh = bpy.data.meshes.new('GEAR_' + key + '_LINE')
    mesh.from_pydata([(0,0,-radii[key]), (0,0,radii[key])], [(0,1)], [])
    line = bpy.data.objects.new('WHEEL_' + key + '.001', mesh)
    bpy.context.scene.collection.objects.link(line)
    line.parent = compression
    line.location = down_offset
    line.EDMProps.SPECIAL_TYPE = 'COLLISION_LINE'

for name, pieces in regions.items():
    vertices, faces = [], []
    for piece in pieces:
        index = {}
        for face in piece:
            indices = []
            for point in face:
                key = tuple(round(v, 7) for v in point)
                if key not in index:
                    index[key] = len(vertices)
                    vertices.append(tuple(point))
                indices.append(index[key])
            faces.append(indices)
    mesh = bpy.data.meshes.new('F23_' + name + '_CONTACT')
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    shell = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(shell)
    shell.parent = root
    shell.EDMProps.SPECIAL_TYPE = 'COLLISION_SHELL'
for name, points in airframe_lines.items():
    mesh = bpy.data.meshes.new('F23_' + name + '_LINE')
    mesh.from_pydata([tuple(p) for p in points], [(i, i + 1) for i in range(len(points) - 1)], [])
    # The exporter truncates at the dot, so a line may share its shell's cell name.
    line = bpy.data.objects.new(name + '.002', mesh)
    bpy.context.scene.collection.objects.link(line)
    line.parent = root
    line.EDMProps.SPECIAL_TYPE = 'COLLISION_LINE'
bpy.context.scene.frame_set(200)
output.parent.mkdir(parents=True, exist_ok=True)
result = bpy.ops.edm.export(filepath=output.as_posix())
if 'FINISHED' not in result or not output.exists():
    raise RuntimeError(result)
output.with_suffix('.json').write_text(json.dumps(dict(
    source_sha256=source_hash, builder_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), regional_parts_blender_m=geometry_receipt, source_hulls_blender_m=source_polys, partition=partition_receipt, collision_sha256=hashlib.sha256(output.read_bytes()).hexdigest(),
    wheel_radius_m=radii, wheel_paths_blender_m=paths,
    airframe_lines_blender_m={k: [list(p) for p in v] for k, v in airframe_lines.items()},
    coordinates='Blender X,Y,Z maps to DCS X,Z,-Y',
    qualification='OFFLINE only; 18 regional hull approximations + three unchanged wheel shells; runtime hit routing required'
), indent=2))
