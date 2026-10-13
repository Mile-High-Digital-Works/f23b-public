# SPDX-License-Identifier: MIT
"""Render original missile Blender sources with their exported base colours."""
import argparse
from pathlib import Path
import sys

import bpy
from mathutils import Matrix, Vector

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--addon-path', required=True)
parser.add_argument('--models', nargs=3, required=True, help='MALICE, HARM, Block II Blender sources')
parser.add_argument('--output', required=True)
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
addon = Path(args.addon_path).resolve()
sys.path.insert(0, str(addon))
from math_tools import ROOT_TRANSFORM_MATRIX as conversion
inverse = conversion.inverted()
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)


def preview_material(name, color):
    material = bpy.data.materials.new(name)
    material.use_nodes = True
    shader = material.node_tree.nodes.get('Principled BSDF')
    shader.inputs['Base Color'].default_value = (*color, 1)
    shader.inputs['Roughness'].default_value = .8
    shader.inputs['Metallic'].default_value = 0
    return material


for path, z, label in zip(args.models, (.68, 0, -.68), ('MALICE', 'PROJECT HARM', 'BLOCK II')):
    with bpy.data.libraries.load(str(Path(path).resolve()), link=False) as (available, loaded):
        loaded.objects = available.objects
    for obj in loaded.objects:
        if obj.type != 'MESH':
            continue
        obj.matrix_world = Matrix.Translation(inverse @ Vector((0, 0, z))) @ obj.matrix_world
        for i, original in enumerate(obj.data.materials):
            color = next(tuple(node.inputs['Base Color'].default_value[:3])
                         for node in original.node_tree.nodes if node.inputs.get('Base Color'))
            obj.data.materials[i] = preview_material('Preview_' + original.name, color)
        bpy.context.collection.objects.link(obj)
    bpy.ops.object.text_add()
    text = bpy.context.object
    text.data.body, text.data.size = label, .064
    text.data.materials.append(preview_material('Label_' + label, (.52, .57, .62)))
    # Text's local XY plane maps to DCS XZ; face the DCS +Y camera.
    text.matrix_world = inverse @ Matrix(((1, 0, 0, -1.55), (0, 0, 1, .19),
                                         (0, -1, 0, z + .44), (0, 0, 0, 1)))

scene = bpy.context.scene
scene.render.engine = 'BLENDER_EEVEE'
scene.render.resolution_x, scene.render.resolution_y = 1600, 1050
scene.render.resolution_percentage = 100
scene.world.use_nodes = True
scene.world.node_tree.nodes.get('Background').inputs[0].default_value = (.055, .065, .075, 1)
scene.world.node_tree.nodes.get('Background').inputs[1].default_value = .7
scene.view_settings.view_transform = 'Standard'
for eye, power in [((0, 4, -3), 450), ((2, 2, 3), 250), ((-2, -2, 0), 300)]:
    bpy.ops.object.light_add(type='AREA', location=inverse @ Vector(eye))
    lamp = bpy.context.object
    lamp.data.energy, lamp.data.size = power, 4
    lamp.rotation_euler = (-lamp.location).to_track_quat('-Z', 'Y').to_euler()
bpy.ops.object.camera_add(location=inverse @ Vector((0, 6, 4)))
camera = bpy.context.object
camera.data.type, camera.data.ortho_scale = 'ORTHO', 4.1
direction = (-camera.location).normalized()
right = inverse @ Vector((1, 0, 0))
up = right.cross(direction)
camera.rotation_euler = Matrix((right, up, -direction)).transposed().to_euler()
scene.camera = camera
output = Path(args.output).resolve()
output.parent.mkdir(parents=True, exist_ok=True)
scene.render.filepath = str(output)
bpy.ops.render.render(write_still=True)
