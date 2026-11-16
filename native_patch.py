# SPDX-License-Identifier: MIT
"""Restore DCS game files changed by earlier F-23B releases, with atomic writes."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

SPECS = {
    'radar': ('aim120_family.lua', 'F23B_AIM424_MALICE.lua',
              '1d2fc016b9a6593b65bfd826042717194e5b4598eb37f9b3c910b341312966f8',
              b'declare_weapon(AIM_120B)', b'local AIM_120B = F23B_AIM424_MALICE\n'),
    'ir': ('aim9_family.lua', 'F23B_AIM9X_BLOCKII.lua',
           'f181ff70c4f593f50c27f61a25dca6014f0f6f93d9aa1df0e1e85b304a0d3990',
           b'declare_weapon(AIM9X)', b'local AIM9X = weapon\n'),
}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def replacement(base, project, kind):
    _, _, expected, boundary, alias = SPECS[kind]
    if sha(base) != expected or base.count(boundary) != 1:
        raise ValueError('Unsupported or modified DCS missile file: ' + kind)
    marker = b'-- F23B_NATIVE_DEFINITION_END'
    if project.count(marker) != 1:
        raise ValueError('Invalid F-23B weapon definition')
    definition = project.split(marker)[0].replace(b'\r\n', b'\n')
    if b'declare_weapon(' in definition or b'declare_loadout(' in definition:
        raise ValueError('Unexpected registration in weapon definition')
    start = base.index(b'local AIM9X =') if kind == 'ir' else 0
    return base[:start] + definition + b'\n' + alias + base[base.index(boundary):]


def atomic_write(path, data):
    temp = path.with_name(path.name + '.f23b-pending')
    try:
        stream = temp.open('xb')
    except FileExistsError:
        raise ValueError('An unfinished write already exists: ' + str(temp))
    try:
        with stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temp, path)
    finally:
        if temp.exists():
            temp.unlink()


def ensure_closed():
    if os.name != 'nt':
        raise ValueError('The native installer requires Windows')
    result = subprocess.run(['tasklist', '/FO', 'CSV', '/NH'], check=True,
                            capture_output=True, text=True)
    for line in result.stdout.lower().splitlines():
        if line.startswith(('"dcs.exe",', '"dcs_updater.exe",', '"modelviewer2.exe",')):
            raise ValueError('Close DCS, its updater and ModelViewer before continuing')


def transact(changes):
    # Preflight BOTH families before either changes. Refuse a concurrent update.
    for target, before, _ in changes:
        if target.read_bytes() != before:
            raise ValueError('DCS file changed: ' + target.name)
    done = []
    try:
        for target, before, after in changes:
            if target.read_bytes() != before:
                raise ValueError('DCS file changed during installation: ' + target.name)
            atomic_write(target, after)
            done.append((target, before, after))
            if target.read_bytes() != after:
                raise ValueError('Write verification failed: ' + target.name)
    except Exception:
        for target, before, after in reversed(done):
            if target.read_bytes() == after:
                atomic_write(target, before)
        raise


def legacy_changes(dcs, allow_newer=False):
    """Restore verified old global replacements before independent weapons load.

    A clean installation needs no backup and causes no game-file writes. Keep
    old receipts/originals for recovery; never derive originals from new weapons.
    """
    dcs = dcs.resolve(strict=True)
    if not (dcs / 'Mods/aircraft/FA-18C/bin/FA18C.dll').is_file():
        raise ValueError('Install the DCS F/A-18C Hornet first')
    backup = dcs / 'F23B-native-backup'
    changes = []
    for kind, (family, _, stock_hash, _, _) in SPECS.items():
        target = dcs / 'CoreMods/aircraft/AircraftWeaponPack' / family
        if allow_newer and not target.exists():
            continue
        if target.resolve(strict=True) != target:
            raise ValueError('Native files must not be redirected: ' + family)
        current = target.read_bytes()
        if sha(current) == stock_hash:
            continue
        if backup.resolve() != backup or backup.is_symlink():
            raise ValueError('Backup directory must not be a link')
        receipt_path = backup / 'receipt.json'
        if not receipt_path.is_file():
            if allow_newer and b'F23B' not in current:
                continue
            raise ValueError('Unsupported or modified DCS missile file: ' + family)
        receipt = json.loads(receipt_path.read_text())
        if (allow_newer and sha(current) != receipt['files'][kind]['patched_sha256']
                and b'F23B' not in current):
            continue
        original_path = backup / family
        if original_path.resolve(strict=True) != original_path:
            raise ValueError('Backup file must not be a link: ' + family)
        original = original_path.read_bytes()
        record = receipt['files'][kind]
        if (receipt['dcs_root'] != str(dcs)
                or sha(original) != stock_hash
                or record['original_sha256'] != stock_hash
                or sha(current) != record['patched_sha256']):
            raise ValueError('Cannot verify the original F-23B replacement: ' + family)
        changes.append((target, current, original))
    return changes


def restore_legacy(dcs):
    transact(legacy_changes(dcs))
    return 'PASS: stock missile definitions retained; verified legacy replacements restored'


def run(action, dcs, package):
    dcs, package = dcs.resolve(strict=True), package.resolve(strict=True)
    if not (dcs / 'Mods/aircraft/FA-18C/bin/FA18C.dll').is_file():
        raise ValueError('Install the DCS F/A-18C Hornet first')
    release = json.loads((package / 'release.json').read_text(encoding='utf-8'))
    backup = dcs / 'F23B-native-backup'
    if backup.is_symlink() or (backup.exists() and backup.resolve() != backup):
        raise ValueError('Backup directory must not be a link')
    receipt_path = backup / 'receipt.json'
    receipt = json.loads(receipt_path.read_text()) if receipt_path.exists() else None
    changes = []
    rows = {}
    revision_changed = False
    for kind, (family, project, _, _, _) in SPECS.items():
        target = dcs / 'CoreMods/aircraft/AircraftWeaponPack' / family
        if target.resolve(strict=True) != target:
            raise ValueError('Native files must not be redirected: ' + family)
        current = target.read_bytes()
        relative = 'F-23B/Weapons/' + project
        definition = (package / relative).read_bytes()
        if sha(definition) != release['files'][relative]:
            raise ValueError('Package weapon checksum mismatch: ' + project)
        if receipt:
            if receipt['dcs_root'] != str(dcs):
                raise ValueError('Backup belongs to another DCS installation')
            base = (backup / family).read_bytes()
            if sha(base) != receipt['files'][kind]['original_sha256']:
                raise ValueError('Backup checksum mismatch: ' + family)
        else:
            base = current
        patched = replacement(base, definition, kind)
        if receipt and sha(patched) != receipt['files'][kind]['patched_sha256']:
            if action != 'install' or current != base:
                raise ValueError('Restore using the original release before installing a different release')
            revision_changed = True
        if action == 'restore':
            if not receipt:
                raise ValueError('No F-23B backup exists for this DCS installation')
            if current not in (base, patched):
                raise ValueError('DCS has changed; refusing to overwrite ' + family)
            changes.append((target, current, base))
        elif action == 'verify':
            if current != patched:
                raise ValueError('Required F-23B patch is missing or changed: ' + family)
        else:
            if current not in (base, patched):
                raise ValueError('DCS has changed; refusing to overwrite ' + family)
            changes.append((target, current, patched))
        rows[kind] = dict(original_sha256=sha(base), patched_sha256=sha(patched))
    if action == 'install' and not receipt:
        # Persist all originals before touching installed content. Never reuse an
        # incomplete backup; it may be needed to recover an interrupted operation.
        backup.mkdir(exist_ok=False)
        for target, before, _ in changes:
            (backup / target.name).write_bytes(before)
        atomic_write(receipt_path, (json.dumps(dict(dcs_root=str(dcs), files=rows), indent=2)+'\n').encode())
    elif action == 'install' and revision_changed:
        if any(sha(before) != rows[kind]['original_sha256']
               for kind, (_, before, _) in zip(SPECS, changes)):
            raise ValueError('Restore both native families before changing release')
        atomic_write(receipt_path, (json.dumps(dict(dcs_root=str(dcs), files=rows), indent=2)+'\n').encode())
    if action != 'verify':
        transact(changes)
    return action.upper() + ' PASS: both native missile families'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('install', 'verify', 'restore'))
    parser.add_argument('--dcs-root', type=Path, required=True)
    parser.add_argument('--package-root', type=Path, default=Path(__file__).resolve().parent)
    args = parser.parse_args()
    try:
        ensure_closed()
        print(run(args.action, args.dcs_root, args.package_root))
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as exc:
        parser.exit(1, 'F-23B: ' + str(exc) + '\n')


if __name__ == '__main__':
    main()
