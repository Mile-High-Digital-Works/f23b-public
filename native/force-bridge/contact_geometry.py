"""Convex-polyhedron clipping in Blender coordinates; project geometry only."""
import math
from mathutils import Vector
EPS=2e-7

def unique(points):
    out={}
    for p in points:out.setdefault(tuple(round(x/EPS) for x in p),p.copy())
    return list(out.values())

def planes(poly):
    out=[];seen=set()
    for face in poly:
        n=max(((face[i]-face[0]).cross(face[i+1]-face[0]) for i in range(1,len(face)-1)),key=lambda v:v.length_squared)
        if n.length<1e-9:continue
        n.normalize();d=n.dot(face[0]);key=tuple(round(x/2e-6) for x in (*n,d))
        if key not in seen:seen.add(key);out.append((n,d))
    return out

def clip(poly,n,d):
    """Closed convex intersection with n dot p <= d. Boundary is shared."""
    if not poly:return []
    n=Vector(n);length=n.length
    if length<1e-12:raise ValueError('Zero clipping normal')
    n/=length;d/=length;faces=[];edge=[]
    for face in poly:
        result=[]
        for a,b in zip(face,face[1:]+face[:1]):
            av=n.dot(a)-d;bv=n.dot(b)-d
            ain=av<=EPS;bin=bv<=EPS
            if ain:result.append(a.copy())
            if ain!=bin:
                q=a+(b-a)*(av/(av-bv));result.append(q);edge.append(q)
        result=unique(result)
        if len(result)>=3:faces.append(result)
    cap=unique(edge)
    if len(cap)>=3:
        center=sum(cap,Vector())/len(cap)
        u=n.cross(Vector((1,0,0)) if abs(n.x)<.9 else Vector((0,1,0))).normalized()
        v=n.cross(u)
        cap.sort(key=lambda p:math.atan2((p-center).dot(v),(p-center).dot(u)))
        faces.append(cap)
    return faces if volume(faces)>1e-9 else []

def volume(poly):
    return abs(sum(face[0].dot(face[i].cross(face[i+1]))/6
                   for face in poly for i in range(1,len(face)-1)))

def intersect(poly,other):
    for n,d in planes(other):
        poly=clip(poly,n,d)
        if not poly:break
    return poly

def subtract(poly,other):
    """Disjoint convex pieces of poly outside another convex polyhedron."""
    pieces=[]
    for n,d in planes(other):
        outside=clip(poly,-n,-d)
        if outside:pieces.append(outside)
        poly=clip(poly,n,d)
        if not poly:break
    return pieces

def hull(points):
    import bmesh
    bm=bmesh.new()
    for p in unique(points):bm.verts.new(p)
    r=bmesh.ops.convex_hull(bm,input=list(bm.verts))
    bmesh.ops.delete(bm,geom=list(set(r['geom_interior']+r['geom_unused'])),context='VERTS')
    bm.normal_update();center=sum((v.co for v in bm.verts),Vector())/len(bm.verts)
    result=[]
    for f in bm.faces:
        p=[v.co.copy() for v in f.verts]
        if (p[1]-p[0]).cross(p[2]-p[0]).dot(p[0]-center)<0:p.reverse()
        result.append(p)
    bm.free();return result

def packed(poly):return [[list(p) for p in f] for f in poly]

def region_parts(body,extras,lines,fins,span):
    seeds={name:sum(lines[name],Vector())/len(lines[name]) for name in
           ('NOSE_CENTER','NOSE_BOTTOM','COCKPIT','FUSELAGE_BOTTOM','FUSELAGE_TOP','TAIL_BOTTOM','ENGINE_L','ENGINE_R')}
    root=2.5;mid=4.1;outer=(4.8+span-.12)/2
    regions={k:[] for k in seeds};sources=[body]+extras
    for name,s in seeds.items():
        for base in sources:
            p=clip(clip(base,(0,1,0),root),(0,-1,0),root)
            for other,t in seeds.items():
                if other!=name:p=clip(p,t-s,(t.length_squared-s.length_squared)/2)
            if p:regions[name].append(p)
    for side,sign in (('L',1),('R',-1)):
        for cell,low,high in (('IN',root,mid),('CENTER',mid,outer),('OUT',outer,span+.001)):
            name='WING_'+side+'_'+cell;regions[name]=[]
            for base in sources:
                p=clip(clip(base,(0,-sign,0),-low),(0,sign,0),high)
                if p:regions[name].append(p)
    # Fins retain their measured hull outside the already assigned airframe.
    fin_cuts={}
    for side,fin in fins.items():
        top=sum(p.z for p in lines['FIN_'+side+'_TOP'])/len(lines['FIN_'+side+'_TOP'])
        center=sum(p.z for p in lines['FIN_'+side+'_CENTER'])/len(lines['FIN_'+side+'_CENTER'])
        cut=(top+center)/2;fin_cuts[side]=cut;parts=subtract(fin,body)
        for cell,n,d in (('TOP',(0,0,-1),-cut),('CENTER',(0,0,1),cut)):
            name='FIN_'+side+'_'+cell;regions[name]=[]
            for part in parts:
                p=clip(part,n,d)
                if p:regions[name].append(p)
    if len(regions)!=18 or any(not p for p in regions.values()):raise RuntimeError('Empty/missing region')
    return regions,{'central_seeds_blender_m':{k:list(v) for k,v in seeds.items()},
                    'wing_cuts_blender_abs_y_m':[root,mid,outer],'fin_cuts_blender_z_m':fin_cuts,
                    'partition_rule':'Measured body hull partitioned by central line-seed Voronoi halfspaces and wing bands; canopy/closed-bay hulls share those cuts; fin portions outside body split by measured height.'}
