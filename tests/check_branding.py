# SPDX-License-Identifier: MIT
"""Validate selected menu pins, and the actual PNGs when runtime assets are supplied."""
import argparse
import io
import json
from pathlib import Path
import sys
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from release_assets import branding

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--runtime-dir', type=Path, help='Folder containing the flown aircraft modules')
args = parser.parse_args()
pin = json.loads((ROOT / 'config/releases/release-v1.5.json').read_text())
manifest = json.loads((ROOT / 'config/releases/menu-v1.5.json').read_text())
expected = {n for n in pin['runtime_files'] if n.endswith('.png') and n not in pin['removed']
            and ('/Theme/' in n or '/Encyclopedia/' in n)}
selected = {item['path']: item for item in manifest['files']}
assert len(selected) == len(manifest['files']), 'Duplicate menu paths'
assert set(selected) == expected, 'Menu manifest differs from the release inventory'
for name, item in selected.items():
    assert item['sha256'] == pin['runtime_files'][name], name
    if '/ME/' in name:
        size = [1920, 1080]
    elif '/Encyclopedia/' in name:
        size = [1280, 720]
    else:
        side = 76 if '76x76' in name else 38 if '38x38' in name else 256
        size = [side, side]
    assert item['size'] == size, name
print(f'PASS: {len(selected)} selected menu pins match the release inventory and dimensions')
if args.runtime_dir:
    graphics = branding(args.runtime_dir)
    assert set(graphics) == expected
    for name, data in graphics.items():
        image = Image.open(io.BytesIO(data))
        image.verify()
        with Image.open(io.BytesIO(data)) as decoded:
            assert list(decoded.size) == selected[name]['size'], name
    print(f'PASS: {len(graphics)} runtime PNGs decode and match the selected hashes and dimensions')
else:
    print('Runtime PNG decoding requires --runtime-dir; packaging also verifies their hashes and dimensions.')
