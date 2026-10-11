# SPDX-License-Identifier: MIT
"""Package the pinned flown aircraft files with the committed source."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import subprocess
import zipfile

from verify_source import verify
from release_assets import branding, fix_mission

ROOT = Path(__file__).resolve().parents[1]
PIN = 'config/releases/release-v1.4.json'
DOCS = 'F-23B-docs/'
TEXT = ('.lua', '.lods', '.txt')
NOTICES = ['LICENCE.md', 'CREDITS.md', 'INSTALL.md',
           'config/licensing/third-party-code-reuse.json',
           'native/flight-feel/docs/provenance/GRINNELLI_V2_1_PERFORMANCE_REFERENCE.md',
           'config/licensing/liveries-v1.4.json', 'docs/PILOT_GUIDE.html']


def sha(data):
    return hashlib.sha256(data).hexdigest()


def zip_bytes(files):
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for name, data in sorted(files.items()):
            info = zipfile.ZipInfo(name, (2026, 10, 6, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            z.writestr(info, data)
    return buffer.getvalue()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime-dir', type=Path, required=True,
                        help='Folder that contains the flown F-23B and F-23B-Player module folders')
    parser.add_argument('--liveries-dir', type=Path, required=True,
                        help='Folder containing the ten pinned F23B livery folders')
    parser.add_argument('--pin', default=PIN,
                        help='Committed config/releases/release-*.json for the accepted runtime')
    args = parser.parse_args()
    if subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT).strip():
        raise SystemExit('Commit the source changes before packaging')
    verify()
    commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    source = subprocess.check_output(['git', 'archive', '--format=zip', 'HEAD'], cwd=ROOT)
    with zipfile.ZipFile(io.BytesIO(source)) as z:
        source_files = {n: z.read(n) for n in z.namelist() if not n.endswith('/')}
    pin_path = Path(args.pin)
    if (pin_path.is_absolute() or '..' in pin_path.parts
            or pin_path.parent.as_posix() != 'config/releases'
            or not pin_path.name.startswith('release-') or pin_path.suffix != '.json'):
        raise SystemExit('The release pin must be a committed config/releases/release-*.json')
    pin = json.loads(source_files[pin_path.as_posix()])
    name = pin['name']

    runtime = {p.relative_to(args.runtime_dir).as_posix(): p.read_bytes()
               for p in args.runtime_dir.rglob('*') if p.is_file()}
    if set(runtime) != set(pin['runtime_files']):
        raise SystemExit('Runtime inventory differs from the pinned flown build')
    for n, expected in pin['runtime_files'].items():
        if sha(runtime[n]) != expected:
            raise SystemExit(f'Runtime hash mismatch: {n}')

    files = {n: b for n, b in runtime.items() if n not in pin['removed']}
    # Every shipped script and configuration file is the committed source file.
    committed = {n[len('Mods/aircraft/'):] for n in source_files if n.startswith('Mods/aircraft/')}
    if committed != {n for n in files if n.endswith(TEXT)}:
        raise SystemExit('Committed module source and shipped text files differ')
    for n in pin['from_source']:
        files[n] = source_files['Mods/aircraft/' + n]
    for n in committed - set(pin['from_source']):
        if files[n] != source_files['Mods/aircraft/' + n]:
            raise SystemExit(f'Shipped file differs from committed source: {n}')
    graphics = branding(args.runtime_dir)
    if not graphics.keys() <= files.keys():
        raise SystemExit('Unexpected menu graphics inventory')
    files.update(graphics)
    missions = [n for n in files if n.endswith('.miz')]
    if not pin.get('runtime_is_final', False):
        for n in missions:
            files[n] = fix_mission(files[n])
    if pin.get('runtime_is_final', False):
        for n, data in files.items():
            if data != runtime[n]:
                raise SystemExit('Final runtime differs from the installed aircraft: ' + n)

    # Both installation methods use this exact profile-relative inventory.
    files = {'Mods/aircraft/' + n: data for n, data in files.items()}
    liveries = {p.relative_to(args.liveries_dir).as_posix(): p.read_bytes()
                for p in args.liveries_dir.rglob('*') if p.is_file()}
    if {n: sha(b) for n, b in liveries.items()} != pin['livery_files']:
        raise SystemExit('Livery inventory or hashes differ from the cleared build')
    for n, data in liveries.items():
        target = 'Liveries/F-23B/' + n
        if n.endswith('.lua') and data != source_files[target]:
            raise SystemExit('Livery script differs from committed source: ' + n)
        files[target] = data

    for n in NOTICES:
        files[DOCS + n] = source_files[n]
    files[DOCS + 'known-installations.json'] = source_files['config/releases/known-installations.json']
    files[DOCS + 'dcs-requirements.json'] = source_files['config/releases/dcs-requirements.json']
    files[DOCS + 'NOTICES.md'] = (
        f'# {name}\n\n'
        'THIS MATERIAL IS NOT MADE OR SUPPORTED BY EAGLE DYNAMICS SA.\n\n'
        'Requires an installed, activated DCS: F/A-18C Hornet. Read INSTALL.md.\n'
        'Software: GPL-3.0-or-later with retained file-level MIT grants.\n'
        'Visual derivatives: SytaPastel YF-23, CGTrader product 2046482.\n'
        'See LICENCE.md and CREDITS.md for full terms and component notices.\n'
    ).encode()
    files[DOCS + 'SOURCE.md'] = (
        '# Corresponding software source\n\n'
        f'Companion archive: `{name}-source.zip`.\n\n'
        f'Repository: https://github.com/Coaokalo/f23b-public/tree/{commit}\n\n'
        'The companion contains source, build scripts, tests, license notices, and\n'
        'BUILDING.md. SDK headers and licensed visual sources are external inputs.\n'
        f'Source archive SHA-256: `{sha(source)}`.\n'
    ).encode()
    manifest = dict(name=name, kind='RELEASE', dcs_version=pin['dcs_version'],
                    description=pin['description'], validation_limits=pin['validation_limits'],
                    source_repository='https://github.com/Coaokalo/f23b-public',
                    corresponding_source_commit=commit, release_tooling_source=commit,
                    corresponding_source_archive=name + '-source.zip',
                    corresponding_source_sha256=sha(source),
                    removed_from_flown_build=sorted(pin['removed']),
                    files={n: sha(b) for n, b in sorted(files.items())})
    files[DOCS + 'release.json'] = (json.dumps(manifest, indent=2) + '\n').encode()

    changed = set(pin['from_source']) | graphics.keys() | set(missions)
    assert all(files['Mods/aircraft/' + n] == runtime[n]
               for n in runtime if n not in pin['removed'] and n not in changed)
    output = ROOT / 'dist' / name / commit[:12]
    output.mkdir(parents=True, exist_ok=True)
    assets = {name + '.zip': zip_bytes(files), name + '-source.zip': source,
              name + '-Pilot-Guide.html': source_files['docs/PILOT_GUIDE.html']}
    for asset, data in assets.items():
        path = output / asset
        if path.exists() and path.read_bytes() != data:
            raise SystemExit(f'Output already exists with different bytes: {path.name}')
        path.write_bytes(data)
    checksums = ''.join(f'{sha(data)}  {asset}\n' for asset, data in sorted(assets.items()))
    (output / 'SHA256SUMS.txt').write_text(checksums, encoding='utf-8')
    (output / 'RELEASE_NOTES.md').write_bytes(source_files['docs/RELEASE_NOTES.md'])
    kept = sum(1 for n in files if n.startswith('Mods/aircraft/'))
    print(f'PASS: {kept} flown module files; {len(pin["removed"])} unused files removed; source commit {commit}')
    print(output)
    print(checksums)


if __name__ == '__main__':
    main()
