# SPDX-License-Identifier: MIT
import io
import hashlib
import json
from pathlib import Path
import re
import sys
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from release_assets import (fix_mission, MISSION_TEXT_OLD, MISSION_TEXT_NEW,
                            MISSION_MODULES_OLD, MISSION_MODULES_NEW, MISSION_LIVERY, branding)


class ContentTests(unittest.TestCase):
    def selected_art_fixture(self, folder):
        runtime = Path(folder) / 'runtime'
        name = 'F-23B/Theme/icon.png'
        data = b'\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR\x00\x00\x00\x01\x00\x00\x00\x01'
        image = runtime / name
        image.parent.mkdir(parents=True)
        image.write_bytes(data)
        manifest = Path(folder) / 'menu.json'
        manifest.write_text(json.dumps({'files': [{'path': name, 'size': [1, 1],
                            'sha256': hashlib.sha256(data).hexdigest()}]}))
        return runtime, manifest, image, name, data

    def test_selected_art_is_preserved_and_changed_pixels_are_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            runtime, manifest, image, name, data = self.selected_art_fixture(folder)
            self.assertEqual(branding(runtime, manifest), {name: data})
            image.write_bytes(data + b'changed')
            with self.assertRaisesRegex(ValueError, 'hash mismatch'):
                branding(runtime, manifest)

    def test_selected_art_dimensions_must_match_the_manifest(self):
        with tempfile.TemporaryDirectory() as folder:
            runtime, manifest, _, _, _ = self.selected_art_fixture(folder)
            value = json.loads(manifest.read_text())
            value['files'][0]['size'] = [2, 1]
            manifest.write_text(json.dumps(value))
            with self.assertRaisesRegex(ValueError, 'dimensions mismatch'):
                branding(runtime, manifest)

    def test_selected_art_cannot_escape_the_runtime(self):
        with tempfile.TemporaryDirectory() as folder:
            runtime, manifest, _, _, _ = self.selected_art_fixture(folder)
            value = json.loads(manifest.read_text())
            value['files'][0]['path'] = '../outside.png'
            manifest.write_text(json.dumps(value))
            with self.assertRaisesRegex(ValueError, 'asset path'):
                branding(runtime, manifest)

    def test_mission_requirements_match_plugins_and_other_content_is_preserved(self):
        original = ('mission = {descriptionText="' + MISSION_TEXT_OLD + '",'
                    'descriptionBlueTask="' + MISSION_TEXT_OLD + '",'
                    + MISSION_MODULES_OLD + ','
                    '["type"] = "F-23B",payload="{F23B-AIM424-MALICE}",triggers={1,2,3}}').encode()
        members = {'mission': original, 'options': b'options = {}',
                   'l10n/DEFAULT/mapResource': b'mapResource = {}'}
        source = io.BytesIO()
        with zipfile.ZipFile(source, 'w') as z:
            for n, data in members.items():
                z.writestr(n, data)
        with zipfile.ZipFile(io.BytesIO(fix_mission(source.getvalue()))) as z:
            self.assertEqual(set(z.namelist()), set(members))
            corrected = z.read('mission')
            plugin_ids = set()
            for module in ('F-23B', 'F-23B-Player'):
                entry = (ROOT / 'Mods/aircraft' / module / 'entry.lua').read_text()
                plugin_ids.add(re.search(r'local self_ID = "([^"]+)"', entry).group(1))
            requirements = re.search(rb'\["requiredModules"\]\s*=\s*\{([^}]+)\}', corrected).group(1)
            self.assertEqual(set(re.findall(r'=\s*"([^"]+)"', requirements.decode())), plugin_ids)
            restored = corrected.replace(MISSION_TEXT_NEW.encode(), MISSION_TEXT_OLD.encode())
            restored = restored.replace(MISSION_MODULES_NEW.encode(), MISSION_MODULES_OLD.encode())
            self.assertIn(MISSION_LIVERY.encode(), corrected)
            restored = restored.replace(MISSION_LIVERY.encode(), b'')
            self.assertEqual(restored, original)
            for n in members.keys() - {'mission'}:
                self.assertEqual(z.read(n), members[n])

    def test_changed_mission_format_is_not_silently_rewritten(self):
        source = io.BytesIO()
        with zipfile.ZipFile(source, 'w') as z:
            z.writestr('mission', 'mission = {descriptionText="different revision"}')
        with self.assertRaises(ValueError):
            fix_mission(source.getvalue())

    def test_unexpected_module_requirements_are_rejected(self):
        source = io.BytesIO()
        with zipfile.ZipFile(source, 'w') as z:
            z.writestr('mission', 'mission = {descriptionText="' + MISSION_TEXT_OLD
                       + '",descriptionBlueTask="' + MISSION_TEXT_OLD
                       + '",["requiredModules"] = { ["different"] = "different" }}')
        with self.assertRaisesRegex(ValueError, 'module requirements'):
            fix_mission(source.getvalue())

    def test_every_shipped_model_reference_is_pinned(self):
        pin = json.loads((ROOT / 'config/releases/release-v1.4.json').read_text())
        shipped = {n for n in pin['runtime_files'] if n not in pin['removed']}
        text = (ROOT / 'Mods/aircraft/F-23B/Shapes/F-23B.lods').read_text()
        models = re.findall(r'"([^"]+\.edm)"', text)
        self.assertTrue(models)
        for name in models:
            self.assertIn('F-23B/Shapes/' + name, shipped)

    def test_removed_files_are_unused_by_shipped_scripts(self):
        pin = json.loads((ROOT / 'config/releases/release-v1.4.json').read_text())
        scripts = [p.read_text(encoding='utf-8', errors='replace')
                   for p in (ROOT / 'Mods/aircraft').rglob('*') if p.is_file()]
        for name in pin['removed']:
            if name.endswith('.lua'):
                self.assertFalse(any(Path(name).parent.name + '/' + Path(name).name in s for s in scripts), name)
            if name.endswith('.png'):
                self.assertFalse(any(Path(name).stem in s for s in scripts), name)


if __name__ == '__main__':
    unittest.main()
