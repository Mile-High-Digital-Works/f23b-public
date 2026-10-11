# SPDX-License-Identifier: MIT
"""Short free calibration manoeuvres using recorded thrust, without an engine model."""
import math
import numpy as np


def initial(source):
    d,a=source['d'],source['a']
    p,b,h=(math.radians(d[k][0])/2 for k in ('pitch_deg','bank_deg','hdg_deg'))
    cp,sp,cb,sb,ch,sh=math.cos(p),math.sin(p),math.cos(b),math.sin(b),math.cos(h),math.sin(h)
    q=[ch*cp*cb+sh*sp*sb,ch*cp*sb-sh*sp*cb,-sh*cp*cb+ch*sp*sb,ch*sp*cb+sh*cp*sb]
    y=np.array(q+[float(d[k][0]) for k in ('vx','vy','vz')]+list(a[0,6:9])+[d['alt_ft'][0]*.3048])
    env=np.r_[a[0,12:15],a[0,26],a[0,9:12],[d[k][0] for k in ('wx','wy','wz')],a[0,28:31]].astype(float)
    return y,env


def short_replay(native,source,name,bounds=None):
    d,a=source['d'],source['a'];ids=np.flatnonzero(d['step']==name)
    if bounds is None:
        if np.any(np.diff(ids)>1):
            raise ValueError('Repeated segment name requires explicit start/end indices: '+name)
        bounds=(ids[0],ids[-1]+1)
    ids=np.arange(bounds[0],min(len(a),bounds[1]+1))
    selected={**source,'a':a[ids],'d':{k:v[ids] for k,v in d.items()}}
    y,e=initial(selected);rows=[]
    for j,i in enumerate(ids):
        e[:3]=a[i,12:15];e[3]=a[i,26];e[4:7]=a[i,9:12];e[10:13]=a[i,28:31]
        u=a[i,20:26].copy()
        # Initialize once. Subsequent recorded states never overwrite the path.
        dt=0 if j==0 else float(d['t'][i]-d['t'][ids[j-1]])
        o=native.step(y,e,u,dt)
        rows.append([d['t'][i]-d['t'][ids[0]],*o])
    return np.array(rows)
