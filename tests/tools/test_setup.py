# SPDX-License-Identifier: MIT
"""Installation transactions use temporary game/profile fixtures only."""
import json
from pathlib import Path
import sys
import unittest
from unittest.mock import patch
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
import setup_f23b as setup
import test_native_installer as native_tests
installer = native_tests.installer


class SetupTests(unittest.TestCase):
    def setUp(self):
        native_tests.NativeInstallerTests.setUp(self)
        self.profile = self.dcs.parent / 'Saved Games/DCS'
        (self.profile / 'Config').mkdir(parents=True)
        (self.profile / 'Config/controls.txt').write_bytes(b'preserved controls')
        (self.profile / 'Missions').mkdir()
        (self.profile / 'Missions/owner.miz').write_bytes(b'preserved mission')
        for module in setup.MODULES:
            entry = self.package / module / 'entry.lua'
            entry.parent.mkdir(parents=True, exist_ok=True)
            entry.write_bytes(b'fixture aircraft')
        self.cockpit = self.dcs / setup.COCKPIT_SCRIPT
        self.cockpit.parent.mkdir(parents=True, exist_ok=True)
        self.cockpit.write_bytes(b'fixture cockpit')
        (self.package / 'dcs-requirements.json').write_text(json.dumps(dict(
            version='fixture', binaries={'Mods/aircraft/FA-18C/bin/FA18C.dll': installer.sha(b'fixture')})))
        files = {('Mods/aircraft/' + p.relative_to(self.package).as_posix()): p.read_bytes()
                 for p in self.package.rglob('*') if p.is_file()
                 and p.relative_to(self.package).parts[0] in setup.MODULES}
        self.old_files = {n: installer.sha(b) for n, b in files.items()}
        files['Liveries/F-23B/F-23B Test/description.lua'] = b'fixture livery'
        files['Liveries/F-23B/F-23B Test/skin.dds'] = b'fixture texture'
        files[setup.DOCS + '/dcs-requirements.json'] = (self.package / 'dcs-requirements.json').read_bytes()
        files[setup.DOCS + '/LICENSE'] = b'fixture license'
        files[setup.DOCS + '/known-installations.json'] = json.dumps({'releases': [
            {'source_commit': setup.OCTOBER_3_RELEASE, 'files': self.old_files}]}).encode()
        release = dict(files={n: installer.sha(b) for n, b in files.items()},
                       corresponding_source_commit='fixture')
        files[setup.MANIFEST] = json.dumps(release).encode()
        self.payload_files = files
        self.archive = self.dcs.parent / 'payload.zip'
        with zipfile.ZipFile(self.archive, 'w') as z:
            for name, data in files.items():
                z.writestr(name, data)
        self.state = self.dcs.parent / 'setup-state'
        for p in (patch.object(setup, 'native_patch', installer), patch.object(installer, 'ensure_closed'),
                  patch.object(setup, 'receipt_directory', return_value=self.state)):
            p.start()
            self.addCleanup(p.stop)

    def setup_action(self, action):
        return setup.perform(action, self.dcs, self.profile, self.archive)

    def run_action(self, action):
        return installer.run(action, self.dcs, self.package)

    def test_complete_install_repair_remove_preserves_user_files(self):
        self.setup_action('install')
        target = self.profile / 'Mods/aircraft/F-23B/entry.lua'
        target.write_bytes(b'damaged')
        self.setup_action('install')
        self.assertEqual(target.read_bytes(), b'fixture aircraft')
        # This release adds nothing to the DCS game folder.
        self.assertEqual(self.cockpit.read_bytes(), b'fixture cockpit')
        for kind, path in self.targets.items():
            self.assertEqual(path.read_bytes(), self.bases[kind])
        self.setup_action('remove')
        for module in setup.MODULES:
            self.assertFalse((self.profile / 'Mods/aircraft' / module).exists())
        self.assertFalse((self.state).exists())
        self.assertEqual(self.cockpit.read_bytes(), b'fixture cockpit')
        for kind, path in self.targets.items():
            self.assertEqual(path.read_bytes(), self.bases[kind])
        self.assertEqual((self.profile / 'Config/controls.txt').read_bytes(), b'preserved controls')
        self.assertEqual((self.profile / 'Missions/owner.miz').read_bytes(), b'preserved mission')

    def test_unsupported_game_rolls_back_new_modules(self):
        self.targets['ir'].write_bytes(b'F23B unknown legacy changes')
        with self.assertRaises(ValueError):
            self.setup_action('install')
        self.assertFalse((self.profile / 'Mods/aircraft/F-23B').exists())
        self.assertFalse((self.state).exists())
        self.assertEqual(self.targets['radar'].read_bytes(), self.bases['radar'])

    def test_repair_after_dcs_update_preserves_native_files(self):
        self.setup_action('install')
        before = (self.state / 'receipt.json').read_bytes()
        self.targets['ir'].write_bytes(b'updated by DCS')
        self.setup_action('install')
        self.assertEqual(self.targets['ir'].read_bytes(), b'updated by DCS')
        self.assertEqual((self.state / 'receipt.json').read_bytes(), before)
        self.assertTrue((self.profile / 'Mods/aircraft/F-23B/entry.lua').is_file())

    def test_remove_refusal_restores_module_folders(self):
        self.setup_action('install')
        self.cockpit.write_bytes(self.cockpit.read_bytes() + setup.HOOK + b'changed hook')
        with self.assertRaises(ValueError):
            self.setup_action('remove')
        self.assertTrue((self.profile / 'Mods/aircraft/F-23B/entry.lua').is_file())
        self.assertTrue((self.state / 'receipt.json').is_file())

    def test_unmanaged_install_and_added_files_are_preserved(self):
        folder = self.profile / 'Mods/aircraft/F-23B'
        folder.mkdir(parents=True)
        with self.assertRaisesRegex(ValueError, 'Move the existing'):
            self.setup_action('install')
        folder.rmdir()
        self.setup_action('install')
        added = folder / 'owner.txt'
        added.write_bytes(b'keep me')
        for action in ('install', 'remove'):
            with self.assertRaisesRegex(ValueError, 'added file'):
                self.setup_action(action)
        self.assertEqual(added.read_bytes(), b'keep me')

    def test_upgrade_restores_legacy_missiles_and_replaces_receipt(self):
        self.run_action('install')
        self.setup_action('install')
        record = self.state / 'receipt.json'
        receipt = json.loads(record.read_text())
        receipt['source_commit'] = setup.LEGACY_RELEASE
        record.write_text(json.dumps(receipt))
        self.run_action('install')
        self.setup_action('install')
        self.assertEqual(json.loads(record.read_text())['source_commit'], 'fixture')
        for kind, path in self.targets.items():
            self.assertEqual(path.read_bytes(), self.bases[kind])

    def test_upgrade_from_hook_release_removes_cockpit_connection(self):
        self.setup_action('install')
        record = self.state / 'receipt.json'
        receipt = json.loads(record.read_text())
        receipt['source_commit'] = setup.HOOK_RELEASE
        record.write_text(json.dumps(receipt))
        self.cockpit.write_bytes(b'fixture cockpit' + setup.HOOK)
        message = self.setup_action('install')
        self.assertEqual(self.cockpit.read_bytes(), b'fixture cockpit')
        self.assertIn('Earlier F-23B changes', message)
        self.assertEqual(json.loads(record.read_text())['source_commit'], 'fixture')

    def test_remove_of_hook_release_removes_cockpit_connection(self):
        self.setup_action('install')
        record = self.state / 'receipt.json'
        receipt = json.loads(record.read_text())
        receipt['source_commit'] = setup.HOOK_RELEASE
        record.write_text(json.dumps(receipt))
        self.cockpit.write_bytes(b'fixture cockpit' + setup.HOOK)
        self.setup_action('remove')
        self.assertEqual(self.cockpit.read_bytes(), b'fixture cockpit')
        self.assertFalse((self.profile / 'Mods/aircraft/F-23B').exists())

    def test_upgrade_from_september_29_release_changes_no_game_files(self):
        self.setup_action('install')
        record = self.state / 'receipt.json'
        receipt = json.loads(record.read_text())
        receipt['source_commit'] = setup.SEPTEMBER_29_RELEASE
        record.write_text(json.dumps(receipt))
        message = self.setup_action('install')
        self.assertIn('No DCS game files were changed', message)
        self.assertEqual(json.loads(record.read_text())['source_commit'], 'fixture')
        self.assertEqual(self.cockpit.read_bytes(), b'fixture cockpit')

    def test_remove_of_september_29_release(self):
        self.setup_action('install')
        record = self.state / 'receipt.json'
        receipt = json.loads(record.read_text())
        receipt['source_commit'] = setup.SEPTEMBER_29_RELEASE
        record.write_text(json.dumps(receipt))
        self.setup_action('remove')
        self.assertFalse((self.profile / 'Mods/aircraft/F-23B').exists())

    def test_upgrade_from_october_1_release_changes_no_game_files(self):
        self.setup_action('install')
        record = self.state / 'receipt.json'
        receipt = json.loads(record.read_text())
        receipt['source_commit'] = setup.OCTOBER_1_RELEASE
        record.write_text(json.dumps(receipt))
        message = self.setup_action('install')
        self.assertIn('No DCS game files were changed', message)
        self.assertEqual(json.loads(record.read_text())['source_commit'], 'fixture')
        self.assertEqual(self.cockpit.read_bytes(), b'fixture cockpit')

    def test_remove_of_october_1_release(self):
        self.setup_action('install')
        record = self.state / 'receipt.json'
        receipt = json.loads(record.read_text())
        receipt['source_commit'] = setup.OCTOBER_1_RELEASE
        record.write_text(json.dumps(receipt))
        self.setup_action('remove')
        self.assertFalse((self.profile / 'Mods/aircraft/F-23B').exists())

    def test_unknown_release_is_refused(self):
        self.setup_action('install')
        record = self.state / 'receipt.json'
        receipt = json.loads(record.read_text())
        receipt['source_commit'] = 'another release'
        record.write_text(json.dumps(receipt))
        with self.assertRaisesRegex(ValueError, 'previous release'):
            self.setup_action('install')

    def test_cockpit_write_failure_rolls_back_legacy_restoration_and_modules(self):
        self.run_action('install')
        self.cockpit.write_bytes(b'fixture cockpit' + setup.HOOK)
        before = {kind: path.read_bytes() for kind, path in self.targets.items()}
        original = installer.atomic_write
        def fail_hook(path, data):
            if path == self.cockpit:
                raise OSError('simulated cockpit write failure')
            return original(path, data)
        with patch.object(installer, 'atomic_write', fail_hook):
            with self.assertRaisesRegex(OSError, 'simulated'):
                self.setup_action('install')
        for kind, path in self.targets.items():
            self.assertEqual(path.read_bytes(), before[kind])
        self.assertEqual(self.cockpit.read_bytes(), b'fixture cockpit' + setup.HOOK)
        self.assertFalse((self.state).exists())
        self.assertFalse((self.profile / 'Mods/aircraft/F-23B').exists())

    def test_new_dcs_build_installs_with_clear_connection_notice(self):
        (self.dcs / 'Mods/aircraft/FA-18C/bin/FA18C.dll').write_bytes(b'new version')
        message = self.setup_action('install')
        self.assertIn('connection is OFF', message)
        self.assertIn('native Hornet cockpit and radar', message)
        self.assertEqual(self.cockpit.read_bytes(), b'fixture cockpit')
        self.assertTrue((self.state).exists())

    def test_remove_after_game_update_keeps_updated_native_files(self):
        self.setup_action('install')
        self.cockpit.write_bytes(b'new DCS cockpit')
        self.targets['ir'].write_bytes(b'new DCS missile')
        self.setup_action('remove')
        self.assertEqual(self.cockpit.read_bytes(), b'new DCS cockpit')
        self.assertEqual(self.targets['ir'].read_bytes(), b'new DCS missile')

    def profile_hashes(self):
        return {p.relative_to(self.profile).as_posix(): installer.sha(p.read_bytes())
                for p in self.profile.rglob('*') if p.is_file()}

    def test_zip_and_setup_install_identical_profile_files(self):
        with zipfile.ZipFile(self.archive) as z:
            z.extractall(self.profile)
        zip_files = self.profile_hashes()
        message = self.setup_action('install')
        self.assertIn('installed', message)
        self.assertEqual(self.profile_hashes(), zip_files)
        self.assertTrue((self.state / 'receipt.json').is_file())
        self.assertFalse((self.profile / setup.STATE).exists())
        self.setup_action('remove')
        self.assertFalse((self.profile / 'Liveries/F-23B/F-23B Test').exists())
        self.assertFalse((self.profile / setup.DOCS).exists())

    def test_adopt_known_older_manual_installation(self):
        for name in self.old_files:
            target = self.profile / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(self.payload_files[name])
        self.setup_action('install')
        self.assertTrue((self.profile / 'Liveries/F-23B/F-23B Test/skin.dds').is_file())
        self.assertEqual(json.loads((self.state / 'receipt.json').read_text())['source_commit'], 'fixture')

    def test_modified_manual_installation_is_preserved_and_refused(self):
        with zipfile.ZipFile(self.archive) as z:
            z.extractall(self.profile)
        (self.profile / 'Mods/aircraft/F-23B/entry.lua').write_bytes(b'owner customization')
        before = self.profile_hashes()
        with self.assertRaisesRegex(ValueError, 'Move the existing'):
            self.setup_action('install')
        self.assertEqual(self.profile_hashes(), before)
        self.assertFalse(self.state.exists())

    def test_unrelated_livery_survives_install_and_remove(self):
        own = self.profile / 'Liveries/F-23B/Owner design/description.lua'
        own.parent.mkdir(parents=True)
        own.write_bytes(b'keep owner livery')
        self.setup_action('install')
        self.setup_action('remove')
        self.assertEqual(own.read_bytes(), b'keep owner livery')

    def test_fresh_install_does_not_request_administrator(self):
        with patch.object(setup, 'is_admin', return_value=False) as admin:
            setup.perform('install', self.dcs, self.profile, self.archive, request_elevation=True)
        admin.assert_not_called()

    def test_elevation_only_for_verified_legacy_cleanup_and_before_changes(self):
        self.cockpit.write_bytes(b'fixture cockpit' + setup.HOOK)
        before = self.profile_hashes()
        with patch.object(setup, 'is_admin', return_value=False):
            with self.assertRaises(setup.ElevationRequired):
                setup.perform('install', self.dcs, self.profile, self.archive, request_elevation=True)
        self.assertEqual(self.profile_hashes(), before)
        self.assertEqual(self.cockpit.read_bytes(), b'fixture cockpit' + setup.HOOK)
        self.assertFalse(self.state.exists())

    def test_upgrade_from_october_3_release(self):
        self.setup_action('install')
        path = self.state / 'receipt.json'
        record = json.loads(path.read_text())
        record['source_commit'] = setup.OCTOBER_3_RELEASE
        path.write_text(json.dumps(record))
        self.setup_action('install')
        self.assertEqual(json.loads(path.read_text())['source_commit'], 'fixture')

    def test_upgrade_from_v14_release(self):
        self.setup_action('install')
        path = self.state / 'receipt.json'
        record = json.loads(path.read_text())
        record['source_commit'] = setup.V14_RELEASE
        path.write_text(json.dumps(record))
        self.setup_action('install')
        self.assertEqual(json.loads(path.read_text())['source_commit'], 'fixture')


if __name__ == '__main__':
    unittest.main()
