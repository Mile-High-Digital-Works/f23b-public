# SPDX-License-Identifier: MIT
"""Self-contained F-23B setup.

Installs both aircraft folders into a Saved Games DCS profile. It adds nothing
to the DCS game folder. It restores game files that earlier F-23B releases changed.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import sys
import tempfile
import zipfile

import native_patch

MODULES = ('F-23B', 'F-23B-Player')
RELEASE_VERSION = 'v1.5'
STATE = '.f23b-install'  # Earlier releases kept their receipt in the DCS profile.
DOCS = 'F-23B-docs'
MANIFEST = DOCS + '/release.json'
OCTOBER_3_RELEASE = '40c67d86716bb002726db2da89c9ddfbffc26358'
V14_RELEASE = 'f2cd004b6ec587cca88dda921eb3e23641232246'
UNMANAGED = 'Move the existing F-23B aircraft, supplied livery and F-23B-docs folders to a backup location, then run setup again.'
CONNECTION_OFF = ('The F-23B weapon and radar connection is OFF for this DCS build. '
                  'MALICE, Block II, radar upgrades and the 640 NM SA scale are unavailable. '
                  'Flight, the native Hornet cockpit and radar, bays, lights and liveries remain available. '
                  'Get a compatible release at https://github.com/Mile-High-Digital-Works/f23b-public/releases/latest.')
LEGACY_RELEASE = '35a471ede1df312a8af8ca10283a7cd7cf7ba95f'
# The September 18 release added HOOK to the Hornet cockpit script. Setup removes it.
HOOK_RELEASE = '804920ad2fc51de498fed81daab072c1351aa6a7'
# The September 29 release changed no DCS game files. Setup replaces its modules.
SEPTEMBER_29_RELEASE = '6f9f317c2752a1973ffa33491b1358ffdbd95ab5'
# The October 1 release used the same installation layout on DCS 2.9.30.
OCTOBER_1_RELEASE = '570e13658115bd2baab097cd8161c4672efc958e'
COCKPIT_SCRIPT = 'Mods/aircraft/FA-18C/Cockpit/Scripts/device_init.lua'
HOOK = b'''
-- BEGIN F23B INDEPENDENT WEAPONS
if get_aircraft_type() == 'F-23B' then
    local path = require('lfs').writedir() .. 'Mods/aircraft/F-23B-Player/Cockpit/Weapons/device_setup.lua'
    local script = loadfile(path)
    if script then script() end
end
-- END F23B INDEPENDENT WEAPONS
'''


def cockpit_changes(dcs):
    """Remove the September 18 cockpit connection. This release adds no connection."""
    target = dcs / COCKPIT_SCRIPT
    if not target.exists():
        return []
    regular_tree(target)
    current = target.read_bytes()
    marker = b'-- BEGIN F23B INDEPENDENT WEAPONS'
    if marker not in current:
        return []
    if current.count(marker) != 1 or not current.endswith(HOOK):
        raise ValueError('The F-23B cockpit connection was modified; preserve it before continuing')
    return [(target, current, current[:-len(HOOK)])]


def check_dcs(dcs, config):
    """A new DCS build disables the connection; it does not prevent installation."""
    hornet = dcs / 'Mods/aircraft/FA-18C/bin/FA18C.dll'
    if not hornet.is_file():
        raise ValueError('Install and activate the DCS F/A-18C Hornet first.')
    mismatches = []
    for name, expected in config['binaries'].items():
        path = dcs / name
        regular_tree(path)
        if not path.is_file() or native_patch.sha(path.read_bytes()) != expected:
            mismatches.append(name)
    return mismatches


def regular_tree(path, recursive=True):
    if path.is_symlink() or path.resolve() != path.absolute():
        raise ValueError('Setup will not replace linked folders: ' + str(path))
    if path.exists() and recursive:
        for child in path.rglob('*'):
            if child.is_symlink() or child.resolve() != child.absolute():
                raise ValueError('Setup will not replace linked files: ' + str(child))


def payload_roots(files):
    """Own individual livery folders, never the player's whole livery directory."""
    roots = set()
    for name in files:
        parts = PurePosixPath(name).parts
        if not parts or name.startswith('/') or '\\' in name or ':' in name or '..' in parts:
            raise ValueError('Invalid installation path')
        if len(parts) > 3 and parts[:2] == ('Mods', 'aircraft') and parts[2] in MODULES:
            roots.add('/'.join(parts[:3]))
        elif len(parts) > 3 and parts[:2] == ('Liveries', 'F-23B'):
            roots.add('/'.join(parts[:3]))
        elif len(parts) > 1 and parts[0] == DOCS:
            roots.add(DOCS)
        else:
            raise ValueError('Unexpected installation path: ' + name)
    return roots


def unpack(archive, destination):
    with zipfile.ZipFile(archive) as source:
        names = source.namelist()
        if len(names) != len(set(n.lower() for n in names)):
            raise ValueError('Duplicate package entries')
        payload_roots(names)
        source.extractall(destination)
    release = json.loads((destination / MANIFEST).read_text(encoding='utf-8'))
    for name, expected in release['files'].items():
        path = destination / name
        if not path.resolve().is_relative_to(destination.resolve()):
            raise ValueError('Invalid manifest path')
        if not path.is_file() or native_patch.sha(path.read_bytes()) != expected:
            raise ValueError('Damaged download: ' + name)
    actual = {p.relative_to(destination).as_posix() for p in destination.rglob('*') if p.is_file()}
    if actual != set(release['files']) | {MANIFEST}:
        raise ValueError('Package inventory mismatch')
    payload_roots(actual)
    for module in MODULES:
        if not (destination / 'Mods/aircraft' / module / 'entry.lua').is_file():
            raise ValueError('Incomplete aircraft download')
    return release


def receipt_directory(profile):
    """Keep setup bookkeeping outside the profile so ZIP and setup payloads are identical."""
    base = Path(os.environ.get('LOCALAPPDATA', tempfile.gettempdir()))
    key = hashlib.sha256(str(profile.resolve()).casefold().encode()).hexdigest()[:24]
    return base / 'F-23B' / 'installs' / key


def installed_files(profile, roots):
    result = {}
    for root in roots:
        folder = profile / root
        regular_tree(folder)
        if folder.is_file():
            raise ValueError(UNMANAGED)
        for file in folder.rglob('*'):
            if file.is_file():
                result[file.relative_to(profile).as_posix()] = native_patch.sha(file.read_bytes())
    return result


def manual_install(profile, candidates):
    for candidate in candidates:
        expected = candidate['files']
        if expected and installed_files(profile, payload_roots(expected)) == expected:
            return dict(source_commit=candidate['source_commit'], files=expected,
                        profile=str(profile))
    raise ValueError(UNMANAGED)


class ElevationRequired(Exception):
    pass


def is_admin():
    if os.name != 'nt':
        return False
    import ctypes
    return bool(ctypes.windll.shell32.IsUserAnAdmin())


def perform(action, dcs, profile, archive, request_elevation=False, receipt_dir=None):
    native_patch.ensure_closed()
    dcs, profile = dcs.resolve(strict=True), profile.resolve(strict=True)
    if dcs == profile or profile.is_relative_to(dcs) or dcs.is_relative_to(profile):
        raise ValueError('Choose the DCS game folder and a separate Saved Games DCS profile')
    if not (profile / 'Config').is_dir() and not (profile / 'Logs').is_dir():
        raise ValueError('Choose your Saved Games DCS profile (the folder containing Config or Logs)')
    regular_tree(profile / 'Mods/aircraft', recursive=False)
    regular_tree(profile / 'Liveries/F-23B', recursive=False)
    state = receipt_dir if receipt_dir is not None else receipt_directory(profile)
    regular_tree(state)
    legacy_state = profile / STATE
    regular_tree(legacy_state)
    receipt_path = state / 'receipt.json'
    legacy_receipt = legacy_state / 'receipt.json'
    if receipt_path.exists() and legacy_receipt.exists():
        raise ValueError('Two F-23B installation records exist; preserve them before continuing.')
    active_receipt = receipt_path if receipt_path.exists() else legacy_receipt
    receipt = json.loads(active_receipt.read_text()) if active_receipt.is_file() else None
    if legacy_state.exists() and not legacy_receipt.is_file():
        raise ValueError('An incomplete installation record exists at ' + str(legacy_state))
    if receipt and receipt['profile'] != str(profile):
        raise ValueError('This installation belongs to a different Saved Games profile')
    if receipt and receipt.get('dcs_root') != str(dcs) and receipt['source_commit'] == LEGACY_RELEASE:
        raise ValueError('Remove the legacy installation with its original DCS folder first.')
    if receipt:
        receipt['files'] = {('Mods/aircraft/' + n if n.split('/')[0] in MODULES else n): h
                            for n, h in receipt['files'].items()}
        payload_roots(receipt['files'])
    work = Path(tempfile.mkdtemp(prefix='.f23b-setup-', dir=profile))
    moved, installed = [], []
    cleanup, game_done = True, False
    receipt_before = receipt_path.read_bytes() if receipt_path.is_file() else None
    changes = []
    try:
        package = work / 'package'
        package.mkdir()
        release = unpack(archive, package)
        files = dict(release['files'])
        files[MANIFEST] = native_patch.sha((package / MANIFEST).read_bytes())
        new_roots = payload_roots(files)
        catalog_path = package / DOCS / 'known-installations.json'
        catalog = json.loads(catalog_path.read_text())['releases'] if catalog_path.exists() else []
        candidates = [dict(source_commit=release['corresponding_source_commit'], files=files)] + catalog
        known = {r['source_commit'] for r in candidates} | {
            LEGACY_RELEASE, HOOK_RELEASE, SEPTEMBER_29_RELEASE, OCTOBER_1_RELEASE, OCTOBER_3_RELEASE, V14_RELEASE}
        if receipt and receipt['source_commit'] not in known:
            raise ValueError('Remove the previous release with its original installer before changing releases')
        if receipt is None and any((profile / n).exists() for n in new_roots):
            receipt = manual_install(profile, candidates)
        if action == 'remove' and receipt is None:
            raise ValueError('No F-23B installation was found in that profile.')
        old_files = receipt['files'] if receipt else {}
        old_roots = payload_roots(old_files)
        roots = old_roots | new_roots if action == 'install' else old_roots
        current = installed_files(profile, roots)
        for name, digest in current.items():
            if name not in old_files:
                raise ValueError(UNMANAGED if receipt is None else
                                 'Preserve this added file outside the installation before continuing: ' + str(profile / name))
            if digest != old_files[name]:
                raise ValueError('Move this changed file to a backup location before continuing: ' + str(profile / name))
        mismatches = []
        if action == 'install':
            mismatches = check_dcs(dcs, json.loads((package / DOCS / 'dcs-requirements.json').read_text()))
        changes = native_patch.legacy_changes(dcs, allow_newer=True) + cockpit_changes(dcs)
        if request_elevation and changes and not is_admin():
            raise ElevationRequired('Earlier F-23B game-file changes require administrator permission to remove.')
        # All paths, inventory, native restorations, and adoption checks precede mutation.
        try:
            for index, name in enumerate(sorted(roots)):
                target = profile / name
                if target.exists():
                    backup = work / ('previous-' + str(index))
                    target.rename(backup)
                    moved.append((backup, target))
            if legacy_state.exists():
                backup = work / 'previous-legacy-record'
                legacy_state.rename(backup)
                moved.append((backup, legacy_state))
            if action == 'install':
                for name in sorted(new_roots):
                    target = profile / name
                    target.parent.mkdir(parents=True, exist_ok=True)
                    (package / name).rename(target)
                    installed.append(target)
            native_patch.transact(changes)
            game_done = True
            if action == 'install':
                record = dict(layout=2, dcs_root=str(dcs), profile=str(profile),
                              source_commit=release['corresponding_source_commit'], files=files)
                state.mkdir(parents=True, exist_ok=True)
                native_patch.atomic_write(receipt_path, (json.dumps(record, indent=2) + '\n').encode())
            elif receipt_path.exists():
                receipt_path.unlink()
                if not any(state.iterdir()):
                    state.rmdir()
        except Exception:
            cleanup = False
            if game_done:
                native_patch.transact([(target, after, before) for target, before, after in reversed(changes)])
            for index, target in enumerate(reversed(installed)):
                target.rename(work / ('failed-' + str(index)))
            for backup, target in reversed(moved):
                target.parent.mkdir(parents=True, exist_ok=True)
                backup.rename(target)
            if receipt_before is not None:
                native_patch.atomic_write(receipt_path, receipt_before)
            elif receipt_path.exists():
                receipt_path.unlink()
            cleanup = True
            raise
    finally:
        if cleanup:
            if not work.resolve().is_relative_to(profile):
                raise ValueError('Refusing cleanup outside the selected profile')
            shutil.rmtree(work, ignore_errors=True)
    game = (' Earlier F-23B changes to DCS game files were removed.' if changes
            else ' No DCS game files were changed.')
    if action == 'remove':
        return 'F-23B removed. Your controls, missions and unrelated liveries were kept.' + (game if changes else '')
    status = (' ' + CONNECTION_OFF) if mismatches else ' Weapon and radar connection: compatible.'
    return f'F-23B {RELEASE_VERSION} installed.' + game + status + ' Start DCS and select an F-23B Quick Start mission.'


def elevated_action(action, dcs, profile):
    """Relaunch only a fully checked legacy cleanup, retaining the selected user receipt path."""
    import ctypes
    from ctypes import wintypes
    class ExecuteInfo(ctypes.Structure):
        _fields_ = [('cbSize', wintypes.DWORD), ('fMask', wintypes.ULONG),
                    ('hwnd', wintypes.HWND), ('lpVerb', wintypes.LPCWSTR),
                    ('lpFile', wintypes.LPCWSTR), ('lpParameters', wintypes.LPCWSTR),
                    ('lpDirectory', wintypes.LPCWSTR), ('nShow', ctypes.c_int),
                    ('hInstApp', wintypes.HINSTANCE), ('lpIDList', ctypes.c_void_p),
                    ('lpClass', wintypes.LPCWSTR), ('hkeyClass', wintypes.HKEY),
                    ('dwHotKey', wintypes.DWORD), ('hIcon', wintypes.HANDLE),
                    ('hProcess', wintypes.HANDLE)]
    with tempfile.TemporaryDirectory(prefix='f23b-elevation-') as temp:
        result_path = Path(temp) / 'result.json'
        params = ([] if getattr(sys, 'frozen', False) else [str(Path(__file__).resolve())])
        params += ['--action', action, '--dcs-root', str(dcs), '--profile', str(profile),
                   '--result', str(result_path), '--receipt-directory', str(receipt_directory(profile))]
        info = ExecuteInfo()
        info.cbSize, info.fMask = ctypes.sizeof(info), 0x40
        info.lpVerb, info.lpFile = 'runas', sys.executable
        info.lpParameters, info.nShow = subprocess.list2cmdline(params), 0
        shell = ctypes.WinDLL('shell32', use_last_error=True)
        shell.ShellExecuteExW.argtypes = [ctypes.POINTER(ExecuteInfo)]
        shell.ShellExecuteExW.restype = wintypes.BOOL
        if not shell.ShellExecuteExW(ctypes.byref(info)):
            raise ValueError('Administrator permission was declined; no installation files were changed.')
        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        kernel.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
        kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        try:
            while kernel.WaitForSingleObject(info.hProcess, 500) == 0x102:
                pass
        finally:
            kernel.CloseHandle(info.hProcess)
        if not result_path.is_file():
            raise ValueError('The elevated setup did not return a result; run setup again to check the installation.')
        result = json.loads(result_path.read_text())
        if not result['ok']:
            raise ValueError(result['message'])
        return result['message']


def defaults():
    profile = Path.home() / 'Saved Games/DCS'
    dcs = ''
    if os.name == 'nt':
        import winreg
        try:
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER,
                                r'Software\Microsoft\Windows\CurrentVersion\Explorer\User Shell Folders') as key:
                saved = Path(os.path.expandvars(winreg.QueryValueEx(key, '{4C5C32FF-BB9D-43B0-B5B4-2D72E54EAAA4}')[0]))
                profiles = [saved / n for n in ('DCS', 'DCS.openbeta') if (saved / n).is_dir()]
                profile = profiles[0] if profiles else saved / 'DCS'
        except OSError:
            pass
        for key_name in (r'Software\Eagle Dynamics\DCS World', r'Software\Eagle Dynamics\DCS World OpenBeta'):
            try:
                with winreg.OpenKey(winreg.HKEY_CURRENT_USER, key_name) as key:
                    dcs = winreg.QueryValueEx(key, 'Path')[0]
                    break
            except OSError:
                pass
    return dcs, str(profile)


def gui(archive):
    import tkinter as tk
    from tkinter import filedialog, messagebox, ttk
    import threading
    import queue

    root = tk.Tk()
    root.title('F-23B Setup')
    root.resizable(False, False)
    frame = ttk.Frame(root, padding=24)
    frame.grid()
    ttk.Label(frame, text='F-23B Black Widow II', font=('Segoe UI', 18, 'bold')).grid(row=0, column=0, columnspan=2, sticky='w')
    ttk.Label(frame, text=f'{RELEASE_VERSION} | Aircraft, weapons and ten liveries', padding=(0, 6, 0, 14)).grid(row=1, column=0, columnspan=2, sticky='w')
    entries = []
    for row, (label, value) in enumerate(zip(('DCS game folder', 'Saved Games DCS profile'), defaults())):
        ttk.Label(frame, text=label).grid(row=2 + row * 2, column=0, sticky='w')
        entry = ttk.Entry(frame, width=65)
        entry.insert(0, value)
        entry.grid(row=3 + row * 2, column=0, pady=(4, 12))
        def browse(field=entry):
            folder = filedialog.askdirectory(parent=root, title='Select folder')
            if folder:
                field.delete(0, tk.END)
                field.insert(0, folder)
        ttk.Button(frame, text='Browse…', command=browse).grid(row=3 + row * 2, column=1, padx=(10, 0), pady=(4, 12))
        entries.append(entry)
    ttk.Label(frame, wraplength=550, text='Requires an installed, activated F/A-18C Hornet. Close DCS before continuing. Tested connection: DCS 2.9.30.28738. Other DCS builds install with the version-dependent connection disabled.\n\n' + f'Installs the same files as F-23B-{RELEASE_VERSION}.zip. Administrator permission is requested only to remove game-file changes from earlier releases.').grid(row=6, column=0, columnspan=2, sticky='w', pady=(0, 18))
    status = tk.StringVar(value='Ready. No separate Python installation is needed.')
    ttk.Label(frame, textvariable=status, wraplength=550).grid(row=8, column=0, columnspan=2, sticky='w', pady=(16, 0))
    results = queue.Queue()
    busy = False
    def finish():
        nonlocal busy
        try:
            ok, text = results.get_nowait()
        except queue.Empty:
            root.after(100, finish)
            return
        busy = False
        install.config(state='normal')
        remove.config(state='normal')
        status.set(text if ok else 'Setup did not complete. See the message for details.')
        (messagebox.showinfo if ok else messagebox.showerror)('F-23B Setup', text, parent=root)
    def start(action):
        nonlocal busy
        paths = [e.get().strip() for e in entries]
        if not all(paths):
            messagebox.showerror('F-23B Setup', 'Select both folders first.', parent=root)
            return
        if action == 'remove' and not messagebox.askyesno('Remove F-23B', 'Remove the F-23B aircraft, supplied liveries and documentation?', parent=root):
            return
        busy = True
        install.config(state='disabled')
        remove.config(state='disabled')
        status.set('Installing the F-23B…' if action == 'install' else 'Removing the F-23B…')
        def worker():
            try:
                try:
                    message = perform(action, Path(paths[0]), Path(paths[1]), archive, request_elevation=True)
                except ElevationRequired:
                    message = elevated_action(action, Path(paths[0]), Path(paths[1]))
                results.put((True, message))
            except Exception as exc:
                results.put((False, str(exc)))
        threading.Thread(target=worker, daemon=True).start()
        root.after(100, finish)
    install = ttk.Button(frame, text='Install / Repair', command=lambda: start('install'))
    install.grid(row=7, column=0, sticky='w')
    remove = ttk.Button(frame, text='Remove', command=lambda: start('remove'))
    remove.grid(row=7, column=1, sticky='e')
    root.protocol('WM_DELETE_WINDOW', lambda: None if busy else root.destroy())
    root.mainloop()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--action', choices=('install', 'remove'))
    parser.add_argument('--dcs-root', type=Path)
    parser.add_argument('--profile', type=Path)
    parser.add_argument('--result', type=Path)
    parser.add_argument('--receipt-directory', type=Path, help=argparse.SUPPRESS)
    args = parser.parse_args()
    archive = Path(__file__).resolve().parent / 'payload.zip'
    if not args.action:
        gui(archive)
        return
    if not args.dcs_root or not args.profile or not args.result:
        parser.error('Automated setup requires --dcs-root, --profile and --result')
    try:
        message = perform(args.action, args.dcs_root, args.profile, archive, receipt_dir=args.receipt_directory)
        result = dict(ok=True, message=message)
    except Exception as exc:
        result = dict(ok=False, message=str(exc))
    args.result.write_text(json.dumps(result, indent=2), encoding='utf-8')
    sys.exit(0 if result['ok'] else 1)


if __name__ == '__main__':
    main()
