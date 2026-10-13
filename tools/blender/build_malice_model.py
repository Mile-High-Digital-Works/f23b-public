# SPDX-License-Identifier: MIT
"""Build an original MALICE visual; optional uniform scale for owner-approved bay fit."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys
import bpy
from mathutils import Vector, Matrix

parser=argparse.ArgumentParser()
parser.add_argument('--addon-path',required=True)
parser.add_argument('--output',required=True)
parser.add_argument('--preview')
parser.add_argument('--preview-views', action='store_true')
parser.add_argument('--scale', type=float, default=.75)
parser.add_argument('--mesh-output')
parser.add_argument('--blend-output')
args=parser.parse_args(sys.argv[sys.argv.index('--')+1:])
assert 0 < args.scale <= 1, 'Scale must be positive and no larger than the reference model'
# Official addon setup, kept self-contained for the mesh's corresponding source.
addon=Path(args.addon_path).resolve()
sys.path.insert(0,str(addon))
sys.path.insert(0,str(addon.parent))
import Materials
sys.modules['materials']=Materials
import addon_utils
addon_utils.modules_refresh()
enabled=addon_utils.enable('io_scene_edm',default_set=True,persistent=True)
if enabled is None:
    assert 'FINISHED' in bpy.ops.preferences.addon_enable(module='io_scene_edm')
from math_tools import ROOT_TRANSFORM_MATRIX
from materials.materials import build_material_descriptions
inverse=ROOT_TRANSFORM_MATRIX.inverted()
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)

def material(name,color):
    m=bpy.data.materials.new(name)
    m.use_nodes=True
    m.node_tree.nodes.clear()
    shader=m.node_tree.nodes.new('EdmDefaultShaderNodeType')
    shader.post_init(build_material_descriptions()['EDM_Default_Material'])
    shader.inputs['Base Color'].default_value=(*color,1)
    output=m.node_tree.nodes.new('ShaderNodeOutputMaterial')
    m.node_tree.links.new(shader.outputs[0],output.inputs['Surface'])
    return m
body=material('F23B_MALICE_body',(0.10,0.125,0.15))
nose=material('F23B_MALICE_radome',(0.045,0.055,0.068))
dark=material('F23B_MALICE_nozzle',(0.018,0.023,0.028))
metal=material('F23B_MALICE_metal',(0.075,0.09,0.105))
yellow=material('F23B_MALICE_yellow',(0.24,0.20,0.105))
brown=material('F23B_MALICE_brown',(0.12,0.075,0.052))
ink=material('F23B_MALICE_stencil',(0.25,0.28,0.30))

def mesh(name,vertices,faces,mat,smooth=False):
    data=bpy.data.meshes.new(name)
    data.from_pydata([inverse@(Vector(v)*args.scale) for v in vertices],[],faces)
    data.update()
    obj=bpy.data.objects.new(name,data)
    bpy.context.collection.objects.link(obj)
    data.materials.append(mat)
    for polygon in data.polygons: polygon.use_smooth=smooth
    data.uv_layers.new(name='UVMap')
    return obj

def lathe(name,rings,mat,caps=True,reverse=False):
    count=64
    v=[]; sections=[]; f=[]
    for x,r in rings:
        section=[]
        for i in range(1 if abs(r)<1e-8 else count):
            section.append(len(v))
            v.append((x,r*math.cos(i*2*math.pi/count),r*math.sin(i*2*math.pi/count)))
        sections.append(section)
    for a,b in zip(sections,sections[1:]):
        for i in range(count):
            j=(i+1)%count
            if len(b)==1: f.append((a[i],a[j],b[0]))
            elif len(a)==1: f.append((a[0],b[j],b[i]))
            else: f.append((a[i],a[j],b[j],b[i]))
    if caps:
        if len(sections[0])>1: f.append(tuple(reversed(sections[0])))
        if len(sections[-1])>1: f.append(tuple(sections[-1]))
    if reverse: f=[tuple(reversed(face)) for face in f]
    return mesh(name,v,f,mat,True)
lathe('MALICE_airframe',[(-2.055,.139),(-2.025,.147),(-1.93,.169),(-1.89,.17),(1.20,.17),(1.22,.168)],body,caps=False)
# Smooth illustrative ogive; dimensions remain bounded by the cleared envelope.
lathe('MALICE_radome',[(1.22,.168)]+[(1.22+.835*i/24,.168*math.cos(i/24*math.pi/2)) for i in range(1,25)],nose)
lathe('MALICE_exit_lip',[(-2.056,.139),(-2.056,.112),(-2.015,.106)],metal,caps=False,reverse=True)
lathe('MALICE_nozzle_wall',[(-2.015,.106),(-1.90,.072)],dark,caps=False,reverse=True)
lathe('MALICE_nozzle_throat',[(-1.899,.072),(-1.897,.072)],dark)

def band(name,x,width,mat,r=.1706):
    return lathe(name,[(x,r),(x+width,r)],mat,caps=False)
band('MALICE_warhead_band',.97,.040,yellow)
band('MALICE_motor_band',-.87,.040,brown)
for i,x in enumerate([-1.87,-.96,-.26,.65,1.195]):
    band('MALICE_joint_'+str(i),x,.006,metal)

def curved_panel(name,x1,x2,a1,a2,mat,r=.171):
    steps=max(2,math.ceil(abs(a2-a1)/.025))
    v=[(x,r*math.cos(a1+(a2-a1)*i/steps),r*math.sin(a1+(a2-a1)*i/steps))
       for x in (x1,x2) for i in range(steps+1)]
    return mesh(name,v,[(i,i+1,steps+2+i,steps+1+i) for i in range(steps)],mat,True)

for k in range(4):
    angle=k*math.pi/2
    for label,x1,x2,half,mat in [('roll_index',-1.12,-1.00,.12,ink),('access_panel',.09,.26,.14,metal)]:
        curved_panel('MALICE_'+label+'_'+str(k),x1,x2,angle-half,angle+half,mat)
        if label=='access_panel':
            curved_panel('MALICE_panel_inset_'+str(k),x1+.006,x2-.006,angle-half+.025,angle+half-.025,body,r=.1713)

def stencil(text,x,angle,size,direction=1):
    bpy.ops.object.text_add()
    obj=bpy.context.object; obj.name='MALICE_label_'+text
    obj.data.body=text; obj.data.size=size; obj.data.space_character=1.15
    obj.data.align_x='LEFT'; obj.data.resolution_u=3
    obj.data.materials.append(ink)
    bpy.ops.object.convert(target='MESH')
    # Wrap the lettering to the skin. Reverse both text axes on the far side
    # so its baseline remains upright while the face normal stays outward.
    text_width=max(v.co.x for v in obj.data.vertices)
    for vertex in obj.data.vertices:
        tx,ty,tz=vertex.co
        a=angle-direction*ty/.1715
        axial=x+(tx if direction==1 else text_width-tx)
        vertex.co=inverse@(Vector((axial,.1715*math.cos(a),.1715*math.sin(a)))*args.scale)
    obj.data.update()
    if not obj.data.uv_layers: obj.data.uv_layers.new(name='UVMap')
for angle,direction in ((math.pi/2,1),(3*math.pi/2,-1)):
    stencil('AIM-424 MALICE',-.62,angle,.043,direction)
    stencil('F-23B / 001',-.61,angle+direction*.24,.022,direction)
# Four swept aft fins; overall span 0.67 m. Planform is explicitly illustrative.
for k in range(4):
    angle=math.pi/4+k*math.pi/2
    outline=[(-1.96,.155),(-.98,.155),(-1.36,.335),(-1.93,.335)]
    center=Vector((sum(x for x,r in outline)/4,sum(r for x,r in outline)/4))
    inset=[tuple(Vector(p).lerp(center,.12)) for p in outline]
    v=[]
    for thickness,ring in [(-.001,outline),(-.006,inset),(.001,outline),(.006,inset)]:
        for x,r in ring:
            v.append((x,r*math.cos(angle)-thickness*math.sin(angle),r*math.sin(angle)+thickness*math.cos(angle)))
    faces=[(7,6,5,4),(12,13,14,15)]
    for i in range(4):
        j=(i+1)%4
        faces += [(i,4+i,4+j,j),(8+i,8+j,12+j,12+i),(i,j,8+j,8+i)]
    mesh('MALICE_fin_'+str(k),v,faces,body)
output=Path(args.output).resolve()
output.parent.mkdir(parents=True,exist_ok=True)
bpy.context.view_layer.update()
quality={'degenerate_triangles':0,'inward_skin_faces':0,'closed_solids':{}}
for obj in bpy.context.scene.objects:
    if obj.type!='MESH': continue
    data=obj.data; data.calc_loop_triangles()
    dcs=[ROOT_TRANSFORM_MATRIX@obj.matrix_world@v.co for v in data.vertices]
    volume=0
    edges={}
    for face in data.polygons:
        for a,b in zip(face.vertices,list(face.vertices[1:])+[face.vertices[0]]):
            edge=tuple(sorted((a,b))); edges[edge]=edges.get(edge,0)+1
    for tri in data.loop_triangles:
        a,b,c=(dcs[i] for i in tri.vertices)
        normal=(b-a).cross(c-a)
        if normal.length<1e-13: quality['degenerate_triangles']+=1
        volume+=a.dot(b.cross(c))/6
        if any(token in obj.name for token in ('label','panel','roll_index')):
            center=(a+b+c)/3
            if normal.dot(Vector((0,center.y,center.z)))<=0: quality['inward_skin_faces']+=1
    if obj.name.startswith('MALICE_fin_') or obj.name=='MALICE_radome':
        assert all(n==2 for n in edges.values()), (obj.name,'open/nonmanifold solid')
        assert volume>0, (obj.name,'inward winding',volume)
        quality['closed_solids'][obj.name]={'outward_winding':True,'volume_m3':volume}
assert quality['degenerate_triangles']==0 and quality['inward_skin_faces']==0,quality
vertices=[ROOT_TRANSFORM_MATRIX@o.matrix_world@v.co for o in bpy.context.scene.objects if o.type=='MESH' for v in o.data.vertices]
bounds=[[min(v[i] for v in vertices),max(v[i] for v in vertices)] for i in range(3)]
size=[b-a for a,b in bounds]
axis_span=2*(.335+.006)/math.sqrt(2)*args.scale
assert abs(size[0]-4.111*args.scale)<1e-4 and size[1]<=axis_span+1e-4 and size[2]<=axis_span+1e-4,size
if args.blend_output:
    blend=Path(args.blend_output).resolve()
    blend.parent.mkdir(parents=True,exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(blend))
result=bpy.ops.edm.export(filepath=str(output))
assert 'FINISHED' in result and output.stat().st_size>1000,result
report=dict(status='OFFLINE_PASS',approximation=True,dcs_bounds_m=bounds,
    mesh_quality=quality,
    visual_scale=args.scale,body_length_m=4.11*args.scale,body_diameter_m=.34*args.scale,span_m=.67*args.scale,
    finish='Project charcoal grey; subdued identification bands and grey stencils; visual only',
    sha256=hashlib.sha256(output.read_bytes()).hexdigest(),bytes=output.stat().st_size,
    provenance='Project-original procedural mesh; owner-approved uniform visual reduction, illustrative fin/nose geometry; simulation physics unchanged',
    bay_clearance='See capacity audit; DCS flight release not observed')
output.with_suffix('.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(json.dumps(report))
if args.preview or args.mesh_output:
    mesh_vertices=[]; mesh_faces=[]
    for o in bpy.context.scene.objects:
        if o.type!='MESH': continue
        offset=len(mesh_vertices)
        mesh_vertices += [list(ROOT_TRANSFORM_MATRIX@o.matrix_world@v.co) for v in o.data.vertices]
        mesh_faces += [[offset+i for i in p.vertices] for p in o.data.polygons]
    mesh_output=Path(args.mesh_output) if args.mesh_output else Path(args.preview).with_suffix('.mesh.json')
    mesh_output.write_text(json.dumps(dict(vertices=mesh_vertices,faces=mesh_faces)),encoding='utf-8')
if args.preview:
    scene=bpy.context.scene
    scene.render.engine='BLENDER_EEVEE'
    scene.render.resolution_x=1100; scene.render.resolution_y=500
    scene.render.resolution_percentage=100
    scene.world.color=(.12,.12,.12)
    bpy.ops.object.camera_add(location=(2.7,-6.5,2.8))
    camera=bpy.context.object
    camera.rotation_euler=(-camera.location).to_track_quat('-Z','Y').to_euler()
    camera.data.type='ORTHO'; camera.data.ortho_scale=5.2*args.scale
    scene.camera=camera
    bpy.ops.object.light_add(type='AREA',location=(0,-3,4))
    light=bpy.context.object; light.data.energy=1000; light.data.shape='DISK'; light.data.size=5
    light.rotation_euler=(-light.location).to_track_quat('-Z','Y').to_euler()
    scene.render.filepath=str(Path(args.preview).resolve())
    bpy.ops.render.render(write_still=True)
    if args.preview_views:
        preview_path=Path(args.preview).resolve()
        for name,location in [('rear',(-4.5,-2.2,1.2)),('side',(0,-7,0)),('opposite',(0,7,0)),('nose',(4.5,-2.0,1.0))]:
            camera.location=location
            camera.rotation_euler=(-camera.location).to_track_quat('-Z','Y').to_euler()
            scene.render.filepath=str(preview_path.with_name(preview_path.stem+'-'+name+'.png'))
            bpy.ops.render.render(write_still=True)
