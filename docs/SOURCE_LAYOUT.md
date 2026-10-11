# Source layout

| Location | Purpose |
| --- | --- |
| `Mods/aircraft/` | Lua and configuration files for the aircraft release |
| `Liveries/F-23B/` | Livery bindings; runtime paint maps remain in the aircraft ZIP |
| `native/radar-helper/` | Version-guarded weapon, radar and SA connection loaded in memory by the aircraft |
| `native/force-bridge/` | Windows native bridge, callback generators, steering, propulsion and control tests |
| `native/flight-core/` | Original measured force model, command-following controls, SDK interface and offline checks |
| `native_patch.py` | Restoration of game files changed by earlier releases, with atomic writes |
| `setup_f23b.py` | Single-window aircraft setup, repair and removal |
| `tools/build_setup.py` | Bundle the Windows setup executable |
| `tests/tools/` | Installer regression tests using temporary fixtures |
| `tools/` | Source verification and release packaging |
| `config/licensing/` | Attribution for included third-party-derived software |
| `config/releases/` | Flown aircraft file inventory, removed files, and supported DCS binaries |
| `docs/images/` | Project screenshots |

Releases to v1.4 had these three folders under `experiments/`. The current build uses the original project core.

The Git tree contains software, documentation, and the documentation captures
listed in `config/releases/documentation-images.json`. Runtime images, textures,
models, missions, and the DLLs belong to the separately prepared aircraft ZIP.
Development history, private working assets, old releases, and agent instruction
files are absent from this repository.
