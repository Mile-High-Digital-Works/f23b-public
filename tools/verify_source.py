# SPDX-License-Identifier: MIT
"""Check the source snapshot, attribution hashes, and Git publication boundary."""
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def verify():
    origin = json.loads((ROOT / 'SOURCE_ORIGIN.json').read_text())
    failures = []
    for name, expected in origin['unchanged_files'].items():
        path = ROOT / name
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            failures.append(f'Source snapshot mismatch: {name}')
    for name, expected in origin.get('candidate_2', {}).get('files', {}).items():
        path = ROOT / name
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            failures.append(f'Candidate source mismatch: {name}')
    reuse = json.loads((ROOT / 'config/licensing/third-party-code-reuse.json').read_text())
    for item in reuse['derived_files']:
        path = ROOT / item['project_path']
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != item['project_sha256']:
            failures.append(f'Attribution hash mismatch: {item["project_path"]}')
    if (ROOT / '.git').exists():
        names = subprocess.check_output(['git', 'ls-files', '-z'], cwd=ROOT).decode().split('\0')
        names = [name for name in names if name]
    else:
        names = [p.relative_to(ROOT).as_posix() for p in ROOT.rglob('*')
                 if p.is_file() and not any(part in {'build', 'dist', '.audit', '__pycache__', '.venv'}
                                            for part in p.relative_to(ROOT).parts)]
    if not names:
        failures.append('No source files found')
    excluded = {'.edm', '.blend', '.fbx', '.dds', '.png', '.jpg', '.wav', '.ogg', '.miz', '.dll', '.exe', '.zip', '.7z', '.pdb'}
    documentation_images = json.loads(
        (ROOT / 'config/releases/documentation-images.json').read_text())['files']
    for name, expected in documentation_images.items():
        path = Path(name)
        if (path.parent.as_posix() != 'docs/images' or path.suffix != '.jpg'
                or name not in names or not (ROOT / name).is_file()
                or hashlib.sha256((ROOT / name).read_bytes()).hexdigest() != expected):
            failures.append(f'Documentation screenshot mismatch: {name}')
    instructions = {'agents.md', 'claude.md', 'copilot-instructions.md', 'current_project.md'}
    for name in names:
        path = Path(name)
        if ((path.suffix.lower() in excluded and name not in documentation_images)
                or path.name.lower() in instructions):
            failures.append(f'Unexpected source-tree member: {name}')
        if any(p in {'vendor-private', '.devstate', '.runtime', '.cursor', '.codex'} for p in path.parts):
            failures.append(f'Private directory in source tree: {name}')
        data = (ROOT / name).read_bytes()
        if data.startswith(b'version https://git-lfs.github.com/spec/v1'):
            failures.append(f'LFS pointer in source tree: {name}')
        if re.search(rb'[A-Za-z]:[/\\]Users[/\\][^/\\\s]+', data):
            failures.append(f'Personal home path in source tree: {name}')
    if failures:
        raise SystemExit('\n'.join(failures))
    print(f'PASS: {len(origin["unchanged_files"])} inherited source hashes, '
          f'{len(reuse["derived_files"])} attribution hashes, {len(names)} source files')


if __name__ == '__main__':
    verify()
