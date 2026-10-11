# Build and test

The aircraft runtime targets Windows x64. Install Visual Studio 2022 with its
C++ build tools, CMake 3.25+, Python 3.11+, and DCS with the activated F/A-18C
Hornet. Use the headers in your own DCS installation; they are not bundled here.

## Flight bridge

From the repository root, replacing the DCS path with your installation:

```powershell
cmake -S native/force-bridge -B build/native -A x64 -DDCS_ROOT="D:/Eagle Dynamics/DCS World" -DF23B_INDEPENDENT_CORE=ON -DFIT_PARAMETERS_DIR="$PWD/native/flight-core"
cmake --build build/native --config Release
ctest --test-dir build/native -C Release --output-on-failure
```

The library is `build/native/Release/F23B_ForceBridge.dll`. It uses the installed
Hornet's native system callbacks. Building this DLL does not create the complete
aircraft download: licensed visual assets are separate inputs.

## Tests without DCS

The flight-model tests use a project-authored interface stub when DCS headers are
unavailable. The stub tests calculations and integration code, not the real SDK
ABI or flight inside DCS.

```powershell
cmake -S native/flight-core -B build/flight -DFIT_PARAMETERS_DIR="$PWD/native/flight-core"
cmake --build build/flight --config Release
ctest --test-dir build/flight -C Release --output-on-failure
python -B -m unittest discover -s tests/tools -v
python -B tools/verify_source.py
lua tests/lua/radar_upgrade_spec.lua
lua tests/lua/exterior_visual_adapter_spec.lua
lua tests/lua/input_bridge_profile_spec.lua
lua tests/lua/engine_visual_registration_spec.lua
lua tests/lua/malice_blockii_hornet_spec.lua
```

The installer tests use temporary fixtures. They do not patch an installed game.
Hosted CI runs these checks on Windows and Linux. The full native bridge and
real SDK checks require a local Windows installation.

## Native radar helper

`F23B_Radar.dll` connects the project weapons, radar support and SA scale in
memory while an F-23B is flown. It checks the exact DCS binaries in
`config/releases/dcs-requirements.json` before it changes anything, and it
restores the original bytes when the session closes. It needs MinGW-w64 `g++`:

```powershell
./native/radar-helper/build.ps1
```

The script runs the native test groups and writes the DLL to `build/native-radar`.

## Release packaging

`tools/package_release.py` packages the aircraft module files of the flown
build. `config/releases/release-v1.5.json` pins every candidate file by
SHA-256 and lists the unused files that the release removes, with reasons.
Every shipped script and configuration file must equal the committed source.
For v1.5, `config/releases/menu-v1.5.json` selects the revised slate. Packaging
reads those exact PNG bytes from the supplied runtime and checks hashes and
dimensions; it does not regenerate the artwork. Font files are not bundled.
The candidate pin sets `runtime_is_final` to true for byte preservation.
This flag does not indicate owner acceptance. Packaging preserves
every module byte, including Quick Start missions, and checks installed parity.
The v1.4 pin remains a historical baseline until v1.5 has passed its flight checks.

```powershell
python -m pip install -r requirements-release.txt
python -B tools/package_release.py --runtime-dir "PATH/TO/FLOWN/Mods/aircraft" --liveries-dir "PATH/TO/PACK/Liveries/F-23B"
```

Select the committed accepted pin with `--pin config/releases/release-<version>.json`.
The flown module folders are a private release input and are not downloaded by CI.
They contain the licensed visual derivatives. The Blender source for these
derivatives remains private; it is not corresponding software source.
Before release, compare both rebuilt DLLs with the accepted runtime by code
section hash. Other sections contain build paths and timestamps, so whole-file
hashes can differ. The v1.5 candidate helper matches; the final bridge code matches its corresponding source rebuild.
Packaging produces a draft candidate; it does not change GitHub visibility or
publish a release.

## Windows setup executable

After committing the source and packaging the aircraft as above:

```powershell
python -m venv .venv
.venv/Scripts/python.exe -m pip install -r requirements-setup.txt
.venv/Scripts/python.exe tools/build_setup.py --aircraft-zip "PATH/TO/F-23B-v1.5.zip"
```

The build uses [PyInstaller](https://pyinstaller.org/en/stable/usage.html) to bundle
Python, Tk, setup and the exact aircraft ZIP into one setup EXE.
Only builders install these dependencies. The EXE and its checksum are placed
beside the aircraft/source archives. The payload must match the committed source.
The GUI requests administrator access only when verified earlier game-file changes need restoration. Command-line fixture runs can operate without
elevation in temporary folders. Setup never downloads code at runtime.
The executable is unsigned unless a release maintainer signs it separately.

Setup installs only into the Saved Games profile. It adds nothing to the DCS
game folder. It removes the September 18 cockpit connection and restores the
September 17 missile files when they are present.

## ZIP/setup parity

Extract the ZIP into a clean profile fixture. Install the compiled EXE into a second
clean fixture with an empty Config directory. Compare both with this command:

```powershell
python tools/verify_installation.py --aircraft-zip "PATH/F-23B-v1.5.zip" --profile "PATH/TO/FIXTURE"
```

The check compares every file path and SHA-256, including documentation and liveries.
Setup receipts remain outside the profile so both methods install identical files.
Repeat against the actual compiled EXE for fresh install, repair, removal, historical
upgrades, matching manual adoption, modified manual refusal, and a mismatched DCS fixture.
Load the extracted ZIP aircraft in DCS and verify the slot, weapons, SA 640 and all liveries.

## Final private v1.5 build

The pin is marked CANDIDATE until the final owner flight and multiplayer check.
The installed defaults include stable handling, low-speed recovery and the dry-power curve.
No tuning file, damage logger or temporary test hook is shipped.
The retired flight model and its tests are absent from the build and source archive.
The source archive, DLL code sections and module manifest identify the same implementation.

The offline library and tests use NumPy for recorded-data checks:

```powershell
cmake -S native/flight-core -B build/core -DFIT_PARAMETERS_DIR="$PWD/native/flight-core"
cmake --build build/core --config Release
ctest --test-dir build/core -C Release --output-on-failure
```
