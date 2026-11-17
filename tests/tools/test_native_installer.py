# SPDX-License-Identifier: MIT
"""Release boundary tests: stale/partial native patches and archive contamination."""
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[2]


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, ROOT / path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


installer = load('public_native', 'native_patch.py')


class NativeInstallerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        # Windows runners can expose TEMP through an 8.3 alias; match the
        # installer's canonical paths so the simulated write failure fires.
        fixture_root = Path(self.temp.name).resolve()
        self.dcs = fixture_root / 'DCS'
        self.package = fixture_root / 'Download'
        hornet = self.dcs / 'Mods/aircraft/FA-18C/bin/FA18C.dll'
        hornet.parent.mkdir(parents=True)
        hornet.write_bytes(b'fixture')
        self.targets, self.bases, self.specs = {}, {}, {}
        files = {}
        for kind, (family, project, _, boundary, alias) in installer.SPECS.items():
            base = (b'-- other native definitions\nlocal AIM9X = {}\n' if kind == 'ir' else b'local AIM_120B = {}\n') + boundary + b'\n-- untouched remaining family\n'
            target = self.dcs / 'CoreMods/aircraft/AircraftWeaponPack' / family
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(base)
            self.targets[kind], self.bases[kind] = target, base
            self.specs[kind] = (family, project, installer.sha(base), boundary, alias)
            relative = 'F-23B/Weapons/' + project
            definition = self.package / relative
            definition.parent.mkdir(parents=True, exist_ok=True)
            definition.write_bytes(b'local weapon = {}\n-- F23B_NATIVE_DEFINITION_END\n')
            files[relative] = installer.sha(definition.read_bytes())
        (self.package / 'release.json').write_text(json.dumps(dict(files=files)))
        patched = patch.object(installer, 'SPECS', self.specs)
        patched.start()
        self.addCleanup(patched.stop)

    def run_action(self, action):
        return installer.run(action, self.dcs, self.package)

    def test_process_check_refuses_dcs_and_maintenance_tools(self):
        for image in ('DCS.exe', 'DCS_server.exe', 'dCs_SeRvEr.ExE',
                      'DCS_updater.exe', 'ModelViewer2.exe'):
            with self.subTest(image=image), patch.object(installer.os, 'name', 'nt'), \
                    patch.object(installer.subprocess, 'run') as run:
                run.return_value.stdout = f'"{image}","1234","Console","1","1,024 K"\n'
                with self.assertRaisesRegex(ValueError, 'Close DCS'):
                    installer.ensure_closed()
                run.assert_called_once_with(['tasklist', '/FO', 'CSV', '/NH'], check=True,
                                            capture_output=True, text=True,
                                            creationflags=getattr(installer.subprocess, 'CREATE_NO_WINDOW', 0))

    def test_process_check_accepts_unrelated_processes(self):
        with patch.object(installer.os, 'name', 'nt'), \
                patch.object(installer.subprocess, 'run') as run:
            run.return_value.stdout = '"other_DCS_server.exe","1234","Console","1","1,024 K"\n'
            installer.ensure_closed()

    def test_install_verify_idempotence_and_restore(self):
        self.run_action('install')
        self.run_action('install')
        self.run_action('verify')
        self.run_action('restore')
        self.run_action('restore')
        for kind, target in self.targets.items():
            self.assertEqual(target.read_bytes(), self.bases[kind])

    def test_independent_install_leaves_clean_game_untouched(self):
        with patch.object(installer, 'atomic_write') as write:
            installer.restore_legacy(self.dcs)
        write.assert_not_called()
        self.assertFalse((self.dcs / 'F23B-native-backup').exists())

    def test_independent_install_restores_legacy_without_new_definitions(self):
        self.run_action('install')
        # Recovery must not attempt to manufacture old bytes from new weapons.
        for path in (self.package / 'F-23B/Weapons').iterdir():
            path.write_bytes(b'independent weapon definition')
        installer.restore_legacy(self.dcs)
        for kind, path in self.targets.items():
            self.assertEqual(path.read_bytes(), self.bases[kind])
        self.assertTrue((self.dcs / 'F23B-native-backup/receipt.json').is_file())

    def test_independent_restore_refuses_unknown_second_family_atomically(self):
        self.run_action('install')
        before = self.targets['radar'].read_bytes()
        self.targets['ir'].write_bytes(b'another modification')
        with self.assertRaises(ValueError):
            installer.restore_legacy(self.dcs)
        self.assertEqual(self.targets['radar'].read_bytes(), before)

    def test_independent_restore_rolls_back_failed_second_write(self):
        self.run_action('install')
        before = {k: p.read_bytes() for k, p in self.targets.items()}
        original_write = installer.atomic_write
        def fail_second(path, data):
            if path == self.targets['ir']:
                raise PermissionError('simulated locked file')
            original_write(path, data)
        with patch.object(installer, 'atomic_write', side_effect=fail_second):
            with self.assertRaises(PermissionError):
                installer.restore_legacy(self.dcs)
        self.assertEqual(before, {k: p.read_bytes() for k, p in self.targets.items()})

    def test_second_family_invalid_changes_neither(self):
        self.targets['ir'].write_bytes(b'new DCS version')
        with self.assertRaises(ValueError):
            self.run_action('install')
        self.assertEqual(self.targets['radar'].read_bytes(), self.bases['radar'])
        self.assertFalse((self.dcs / 'F23B-native-backup').exists())

    def test_restore_refuses_updated_file_without_touching_other(self):
        self.run_action('install')
        radar = self.targets['radar'].read_bytes()
        self.targets['ir'].write_bytes(b'updated by DCS')
        with self.assertRaises(ValueError):
            self.run_action('restore')
        self.assertEqual(self.targets['radar'].read_bytes(), radar)

    def test_second_write_failure_rolls_back_first(self):
        original = installer.atomic_write

        def fail_second(path, data):
            if path == self.targets['ir']:
                raise PermissionError('simulated locked second family')
            original(path, data)

        with patch.object(installer, 'atomic_write', side_effect=fail_second):
            with self.assertRaises(PermissionError):
                self.run_action('install')
        for kind, target in self.targets.items():
            self.assertEqual(target.read_bytes(), self.bases[kind])
        self.run_action('install')  # saved originals allow a successful retry

    def test_corrupt_backup_refused(self):
        self.run_action('install')
        (self.dcs / 'F23B-native-backup/aim9_family.lua').write_bytes(b'corrupted')
        before = {k: p.read_bytes() for k, p in self.targets.items()}
        with self.assertRaises(ValueError):
            self.run_action('restore')
        self.assertEqual(before, {k: p.read_bytes() for k, p in self.targets.items()})

    def test_package_definition_tampering_refused(self):
        (self.package / 'F-23B/Weapons/F23B_AIM9X_BLOCKII.lua').write_bytes(b'tampered')
        with self.assertRaises(ValueError):
            self.run_action('install')
        self.assertEqual(self.targets['radar'].read_bytes(), self.bases['radar'])

    def test_different_release_requires_restore_then_installs(self):
        self.run_action('install')
        self.run_action('restore')
        name = 'F-23B/Weapons/F23B_AIM9X_BLOCKII.lua'
        definition = self.package / name
        definition.write_bytes(b'local weapon = {changed=true}\n-- F23B_NATIVE_DEFINITION_END\n')
        release = json.loads((self.package / 'release.json').read_text())
        release['files'][name] = installer.sha(definition.read_bytes())
        (self.package / 'release.json').write_text(json.dumps(release))
        self.run_action('install')
        self.run_action('verify')
        self.run_action('restore')


if __name__ == "__main__":
    unittest.main()
