"""Stage the missile delta on the verified control-vibration preview, retaining its DLL."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
REPO=Path(__file__).resolve().parents[2]
BASE_SOURCE='8dd2cf7cef70c8d822efdc26ae20de1ce5f6b6bc'
sys.path.insert(0,str(REPO/'tools'))
from test_active_runtime import check_weapon_missions,check_hornet
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--base-stage',type=Path,default=REPO/'.devstate/native-propulsion/aim260-8dd2cf7cef70')
parser.add_argument('--review',type=Path,required=True)
args=parser.parse_args()
def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest().upper()
source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=REPO,text=True).strip()
if subprocess.check_output(['git','status','--porcelain'],cwd=REPO,text=True).strip():
    raise ValueError('Commit source before staging')
review=json.loads((args.review/'receipt.json').read_text(encoding='utf-8'))
if review['source_commit']!=source: raise ValueError('Review source mismatch')
for row in review['files'].values():
    if digest(REPO/'Mods/aircraft/F-23B/Weapons'/row['project'])!=row['project_sha256'].upper():
        raise ValueError('Reviewed missile definition changed')
base=args.base_stage.resolve(strict=True)
output=REPO/'.devstate/native-propulsion'/('malice-'+source[:12])
if output.exists(): raise FileExistsError(output)
previous={}
for folder,name in [('F-23B','manifest.json'),('F-23B-Player','player-manifest.json')]:
    manifest=json.loads((base/name).read_text(encoding='utf-8-sig'))
    if manifest['source_commit']!=BASE_SOURCE: raise ValueError('Unexpected base build')
    expected={r['path']:r for r in manifest['files']}
    actual={p.relative_to(base/folder).as_posix():p for p in (base/folder).rglob('*') if p.is_file()}
    if expected.keys()!=actual.keys(): raise ValueError('Base inventory changed')
    for name,p in actual.items():
        if not p.resolve().is_relative_to(base) or digest(p)!=expected[name]['sha256'].upper():
            raise ValueError('Base hash/path mismatch '+name)
        previous[folder+'/'+name]=digest(p)
output.mkdir()
for folder in ('F-23B','F-23B-Player'): shutil.copytree(base/folder,output/folder)
def bounds(data, field):
    start=data.index(b'    '+field+b' = {')
    end=data.index(b'\n    },',start)+len(b'\n    },')
    return start,end
aircraft=output/'F-23B/F-23B.lua'
old=aircraft.read_bytes(); src=(REPO/'Mods/aircraft/F-23B/F-23B.lua').read_bytes().replace(b'\r\n',b'\n')
for field in (b'Pylons', b'net_animation', b'lights_data'):
    a,b=bounds(src,field); x,y=bounds(old,field); table=src[a:b]
    if b'\r\n' in old: table=table.replace(b'\n',b'\r\n')
    old=old[:x]+table+old[y:]
import re
rcs = re.search(rb'(?m)^    RCS = [^\r\n]+', src)
assert rcs and len(re.findall(rb'(?m)^    RCS = [^\r\n]+', old)) == 1
old = re.sub(rb'(?m)^    RCS = [^\r\n]+', lambda _: rcs[0], old)
aircraft.write_bytes(old)
changed={'F-23B/F-23B.lua'}
adapter='F-23B-Player/Cockpit/ExteriorVisuals/ExteriorVisualAdapter.lua'
shutil.copyfile(REPO/'Mods/aircraft'/adapter,output/adapter)
changed.add(adapter)
files=['entry.lua','UnitPayloads/F-23B.lua','README.md','Shapes/F-23B-visual.edm','Weapons/F23B_AIM424_MALICE.lua',
       'Weapons/F23B_AIM9X_BLOCKII.lua','Shapes/F23B_MALICE.edm']
inventory=json.loads((REPO/'config/release/hornet-runtime-files.json').read_text(encoding='utf-8'))
files += [p for p in inventory['modules']['F-23B'] if p.endswith('.miz')]
for name in files:
    dest=output/'F-23B'/name; dest.parent.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(REPO/'Mods/aircraft/F-23B'/name,dest); changed.add('F-23B/'+name)
asset_lock=json.loads((REPO/'ASSET_PAYLOAD_LOCK.json').read_text(encoding='utf-8'))
for row in asset_lock['files']:
    if row['runtime_path']=='Mods/aircraft/F-23B/Shapes/F-23B-visual.edm':
        if digest(output/'F-23B/Shapes/F-23B-visual.edm')!=row['sha256'].upper():
            raise ValueError('MALICE bay visual differs from the asset lock')
obsolete=output/'F-23B/Weapons/F23B_AIM260A.lua'
assert obsolete.resolve().is_relative_to(output.resolve())
obsolete.unlink(); changed.add('F-23B/Weapons/F23B_AIM260A.lua')
identity=output/'F-23B-Player/Cockpit/ExteriorVisuals/BuildInfo.lua'
build='native-'+source[:12]+'-malice-blockii'
identity.write_text("-- Generated package identity\nreturn {source_commit='"+source+"', build_id='"+build+"', asset_lock_sha256='"+digest(REPO/'ASSET_PAYLOAD_LOCK.json')+"'}\n",encoding='utf-8')
changed.add('F-23B-Player/Cockpit/ExteriorVisuals/BuildInfo.lua')
for name,hash_value in previous.items():
    if name not in changed and digest(output/name)!=hash_value: raise ValueError('Unrelated byte changed '+name)
lua='D:/Eagle Dynamics/DCS World/bin/luae.exe'
env={**os.environ,'F23B_RUNTIME_ROOT':output.as_posix()}
for folder in ('F-23B','F-23B-Player'):
    for path in (output/folder).rglob('*.lua'):
        subprocess.run([lua,'-e','assert(loadfile(os.getenv("F23B_COMPILE_FILE")))'],
                       env={**env,'F23B_COMPILE_FILE':str(path)},check=True)
for spec in ('malice_blockii_hornet_spec.lua','input_bridge_profile_spec.lua','exterior_visual_adapter_spec.lua'):
    subprocess.run([lua,str(REPO/'tests/lua'/spec)],cwd=REPO,env=env,check=True)
check_weapon_missions(lua,output)
subprocess.run([lua,str(REPO/'tests/lua/malice_native_patch_spec.lua'),str(args.review/'radar-patched.lua'),str(args.review/'ir-patched.lua')],cwd=REPO,env=env,check=True)
compatibility=check_hornet(lua,Path('D:/Eagle Dynamics/DCS World'))
command=r'''
Import-Module (Join-Path $env:F23B_REPO 'tools/lib/F23BDevRuntime.psm1') -Force -DisableNameChecking
foreach ($folder in @('F-23B','F-23B-Player')) {
    $filename = if ($folder -eq 'F-23B') {'manifest.json'} else {'player-manifest.json'}
    $manifest = New-F23BBuildManifest -PackageRoot (Join-Path $env:F23B_STAGE $folder) -SourceCommit $env:F23B_SOURCE -BuildId $env:F23B_BUILD -ModuleRelativePath $folder
    Write-F23BJsonAtomic -Path (Join-Path $env:F23B_STAGE $filename) -Value $manifest
    Test-F23BPackageAgainstManifest -PackageRoot (Join-Path $env:F23B_STAGE $folder) -Manifest $manifest | Out-Null
}
'''
subprocess.run(['pwsh','-NoProfile','-Command',command],check=True,env={**env,'F23B_REPO':str(REPO),'F23B_STAGE':str(output),'F23B_SOURCE':source,'F23B_BUILD':build})
receipt=dict(status='OFFLINE_PASS',source_commit=source,base_source=BASE_SOURCE,
    changed_files=sorted(changed),preserved_files=len(set(previous)-changed),dcs_launched=False,
    native_patch_status=review['status'],hornet_compatibility=compatibility)
(output/'validation.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
print('OFFLINE PASS: '+str(output))
