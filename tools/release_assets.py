# SPDX-License-Identifier: MIT
"""Release content checks and byte-preserving selected menu artwork."""
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import struct
import zipfile

F35_TEXTURE_PREFIX = 'F-23B/Textures/F23A_V11_'
MISSION_TEXT_OLD = 'Coffin load: 2x AIM-9X, 3x AIM-120C'
MISSION_TEXT_NEW = 'Bay load: 2x AIM-9X Block II, 3x AIM-424 MALICE'
MISSION_MODULES_OLD = '["requiredModules"] = { ["F-23B"] = "F-23B" }'
MISSION_MODULES_NEW = ('["requiredModules"] = { ["F-23B Core"] = "F-23B Core", '
                       '["F-23B Player"] = "F-23B Player" }')
MISSION_AIRCRAFT = '["type"] = "F-23B",'
MISSION_LIVERY = '["livery_id"] = "F23B-01-usaf-langley", '


def fix_mission(data):
    """Correct briefing and plugin requirements; preserve other mission content."""
    output = io.BytesIO()
    with zipfile.ZipFile(io.BytesIO(data)) as source:
        text = source.read('mission').decode('utf-8')
        if text.count(MISSION_TEXT_OLD) != 2:
            raise ValueError('Unexpected Quick Start briefing; refusing a broad replacement')
        if text.count(MISSION_MODULES_OLD) != 1:
            raise ValueError('Unexpected Quick Start module requirements')
        text = text.replace(MISSION_TEXT_OLD, MISSION_TEXT_NEW)
        text = text.replace(MISSION_MODULES_OLD, MISSION_MODULES_NEW)
        if text.count(MISSION_AIRCRAFT) != 1:
            raise ValueError('Unexpected Quick Start player aircraft')
        text = text.replace(MISSION_AIRCRAFT, MISSION_LIVERY + MISSION_AIRCRAFT)
        with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as target:
            for item in source.infolist():
                target.writestr(item, text.encode('utf-8') if item.filename == 'mission'
                               else source.read(item.filename))
    return output.getvalue()


def branding(runtime_dir, manifest_path=None):
    """Read the selected, hash-pinned pictures without regenerating their pixels."""
    if manifest_path is None:
        manifest_path = Path(__file__).resolve().parents[1] / 'config/releases/menu-v1.5.json'
    manifest = json.loads(Path(manifest_path).read_text(encoding='utf-8'))
    runtime_dir = Path(runtime_dir).resolve()
    files = {}
    for item in manifest['files']:
        name = item['path']
        relative = PurePosixPath(name)
        if (not relative.parts or relative.is_absolute() or '..' in relative.parts or '\\' in name
                or relative.parts[0] != 'F-23B' or relative.suffix != '.png'
                or name in files):
            raise ValueError('Unexpected menu asset path: ' + name)
        path = (runtime_dir / name).resolve()
        if not path.is_relative_to(runtime_dir):
            raise ValueError('Menu asset resolves outside the runtime: ' + name)
        data = path.read_bytes()
        if hashlib.sha256(data).hexdigest() != item['sha256']:
            raise ValueError('Selected menu asset hash mismatch: ' + name)
        if (len(data) < 24 or data[:8] != b'\x89PNG\r\n\x1a\n'
                or data[12:16] != b'IHDR'
                or list(struct.unpack('>II', data[16:24])) != item['size']):
            raise ValueError('Selected menu asset dimensions mismatch: ' + name)
        files[name] = data
    if not files:
        raise ValueError('Selected menu asset manifest is empty')
    return files
