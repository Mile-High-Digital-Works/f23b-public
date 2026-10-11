"""Apply only the AIM-260 delta to the manifest-verified e19a79f speed preview."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

REPO = Path(__file__).resolve().parents[2]
BASE_SOURCE = "e19a79f8d134dcc09a825432454a7a2f8fa1b18c"
sys.path.insert(0, str(REPO / "tools"))
from test_active_runtime import check_lua, check_weapon_missions, check_hornet


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-stage", type=Path, default=REPO / ".devstate/native-propulsion/speed-e19a79f8d134")
    parser.add_argument("--dcs-root", type=Path, default=Path("D:/Eagle Dynamics/DCS World"))
    parser.add_argument("--rebuild-flight-bridge", action="store_true",
                        help="Build/test this source's flight DLL and include it with the missile preview")
    args = parser.parse_args()
    source = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=REPO, text=True).strip()
    if subprocess.check_output(["git", "status", "--porcelain"], cwd=REPO, text=True).strip():
        raise ValueError("Commit the AIM-260 source before staging")
    base = args.base_stage.resolve(strict=True)
    output = REPO / ".devstate/native-propulsion" / ("aim260-" + source[:12])
    if output.exists():
        raise FileExistsError(output)
    previous = {}
    for folder, filename in (("F-23B", "manifest.json"), ("F-23B-Player", "player-manifest.json")):
        manifest = json.loads((base / filename).read_text(encoding="utf-8-sig"))
        if manifest["source_commit"] != BASE_SOURCE:
            raise ValueError("Expected the e19a79f speed preview")
        expected = {row["path"]: row for row in manifest["files"]}
        actual = {p.relative_to(base / folder).as_posix(): p for p in (base / folder).rglob("*") if p.is_file()}
        if set(expected) != set(actual):
            raise ValueError("Base inventory mismatch: " + folder)
        for name, path in actual.items():
            if not path.resolve().is_relative_to(base):
                raise ValueError("Base file escapes stage")
            row = expected[name]
            if path.stat().st_size != row["bytes"] or digest(path) != row["sha256"].upper():
                raise ValueError("Base hash mismatch: " + name)
            previous[folder + "/" + name] = digest(path)
    output.mkdir()
    for folder in ("F-23B", "F-23B-Player"):
        shutil.copytree(base / folder, output / folder)

    # Retain the preview's measured gear, simplified damage and force registration.
    aircraft = output / "F-23B/F-23B.lua"
    old = aircraft.read_bytes()
    stock = b"{40EF17B7-F508-45de-8566-6FFECC0C1AB8}"
    if old.count(stock) != 3:
        raise ValueError("Expected exactly three AMRAAM stations in preview")
    # Transfer only the station table; require removal of the new AIM-260
    # choices to reproduce the original preview table exactly.
    def station_bounds(data):
        start = data.index(b"    Pylons = {")
        end = data.index(b"\n    },", start) + len(b"\n    },")
        return start, end
    src = (REPO / "Mods/aircraft/F-23B/F-23B.lua").read_bytes().replace(b"\r\n", b"\n")
    first, last = station_bounds(src)
    table = src[first:last]
    custom = b'''            { CLSID = "{F23B-AIM260A}",
              attach_point_position = { 0.15, -0.06, 0.0 },
              Cx_gain_empty = 0.0, Cx_gain_item = 0 },
'''
    first, last = station_bounds(old)
    if table.count(custom) != 3 or table.replace(custom, b"") != old[first:last].replace(b"\r\n", b"\n"):
        raise ValueError("Station changes extend beyond adding AIM-260 choices")
    if b"\r\n" in old:
        table = table.replace(b"\n", b"\r\n")
    aircraft.write_bytes(old[:first] + table + old[last:])
    changed = {"F-23B/F-23B.lua"}
    files = ["entry.lua", "Weapons/F23B_AIM260A.lua", "UnitPayloads/F-23B.lua", "README.md"]
    inventory = json.loads((REPO / "config/release/hornet-runtime-files.json").read_text())
    files += [p for p in inventory["modules"]["F-23B"] if p.endswith(".miz")]
    entry = (REPO / "Mods/aircraft/F-23B/entry.lua").read_text()
    loader = 'dofile(current_mod_path .. "/Weapons/F23B_AIM260A.lua")\n'
    if entry.count(loader) != 1 or entry.replace(loader, "") != (base / "F-23B/entry.lua").read_text():
        raise ValueError("Core entry changed beyond the AIM-260 loader")
    for name in files:
        target = output / "F-23B" / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(REPO / "Mods/aircraft/F-23B" / name, target)
        changed.add("F-23B/" + name)
    identity = output / "F-23B-Player/Cockpit/ExteriorVisuals/BuildInfo.lua"
    build_id = "native-" + source[:12] + "-aim260"
    identity.write_text("-- Generated package identity\nreturn { source_commit = '" + source
                        + "', build_id = '" + build_id + "', asset_lock_sha256 = '"
                        + digest(REPO / "ASSET_PAYLOAD_LOCK.json") + "' }\n")
    changed.add("F-23B-Player/Cockpit/ExteriorVisuals/BuildInfo.lua")
    if args.rebuild_flight_bridge:
        build = REPO / ".devstate/native-propulsion/build-bridge"
        subprocess.run(["cmake", "-S", str(Path(__file__).parent), "-B", str(build),
                        "-G", "Visual Studio 17 2022", "-A", "x64",
                        "-DDCS_ROOT=" + str(args.dcs_root)], check=True)
        subprocess.run(["cmake", "--build", str(build), "--config", "Release"], check=True)
        subprocess.run(["ctest", "--test-dir", str(build), "-C", "Release",
                        "--output-on-failure"], check=True)
        dll = "F-23B-Player/bin/F23B_ForceBridge.dll"
        shutil.copyfile(build / "Release/F23B_ForceBridge.dll", output / dll)
        changed.add(dll)
    for name, sha in previous.items():
        if name not in changed and digest(output / name) != sha:
            raise ValueError("Unrelated preview byte changed: " + name)

    lua = str(args.dcs_root / "bin/luae.exe")
    check_lua(lua)
    env = {**os.environ, "F23B_RUNTIME_ROOT": output.as_posix()}
    for folder in ("F-23B", "F-23B-Player"):
        for path in (output / folder).rglob("*.lua"):
            subprocess.run([lua, "-e", 'assert(loadfile(os.getenv("F23B_COMPILE_FILE")))'],
                           env={**env, "F23B_COMPILE_FILE": str(path)}, check=True)
    for spec in ("aim260_hornet_spec.lua", "input_bridge_profile_spec.lua", "exterior_visual_adapter_spec.lua"):
        subprocess.run([lua, str(REPO / "tests/lua" / spec)], cwd=REPO, env=env, check=True)
    check_weapon_missions(lua, output)
    compatibility = check_hornet(lua, args.dcs_root)
    # Use the production manifest implementation, including its canonical tree hash.
    command = r"""
Import-Module (Join-Path $env:F23B_REPO 'tools/lib/F23BDevRuntime.psm1') -Force -DisableNameChecking
foreach ($folder in @('F-23B', 'F-23B-Player')) {
    $filename = if ($folder -eq 'F-23B') { 'manifest.json' } else { 'player-manifest.json' }
    $manifest = New-F23BBuildManifest -PackageRoot (Join-Path $env:F23B_STAGE $folder) -SourceCommit $env:F23B_SOURCE -BuildId $env:F23B_BUILD -ModuleRelativePath $folder
    Write-F23BJsonAtomic -Path (Join-Path $env:F23B_STAGE $filename) -Value $manifest
    Test-F23BPackageAgainstManifest -PackageRoot (Join-Path $env:F23B_STAGE $folder) -Manifest $manifest | Out-Null
}
"""
    subprocess.run(["pwsh", "-NoProfile", "-Command", command], check=True,
                   env={**os.environ, "F23B_REPO": str(REPO), "F23B_STAGE": str(output),
                        "F23B_SOURCE": source, "F23B_BUILD": build_id})
    receipt = {"status": "OFFLINE_PASS", "source_commit": source, "base_source": BASE_SOURCE,
               "base_stage": str(base), "changed_files": sorted(changed),
               "preserved_files": len(set(previous) - changed), "dcs_launched": False,
               "flight_bridge_rebuilt": args.rebuild_flight_bridge,
               "hornet_compatibility": compatibility}
    (output / "validation.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print("OFFLINE PASS: AIM-260 preview staged at " + str(output))


if __name__ == "__main__":
    main()
