# Third-party notices

## Grinnelli Designs F-22A performance source

The flight-model adaptation uses GPL-covered source from
[Grinnelli Designs F-22A](https://github.com/grinnellidesigns/f-22a), tag `v2.1.0`,
commit `24dc1f51a8d0d9427c7bd3c368ccabd3e0ade53c`.

Copyright (C) 2025 Branden Hooper (upstream portions/behavior).
Copyright (C) 2026 F-23B project contributors (adaptation).

The relevant RAPTOR.h and RAPTOR.cpp notices specify GPL-3.0-or-later. Their
file-specific notices are retained; the upstream EFM directory's separate MIT
notice does not replace them. The combined software is GPL-3.0-or-later, with
existing project-original MIT grants retained at file level. License texts are
in [COPYING](COPYING) and [LICENSES/MIT.txt](LICENSES/MIT.txt).

Exact upstream hashes, included derived files, and modification records are in
[the reuse manifest](config/licensing/third-party-code-reuse.json). The
[performance provenance record](native/flight-feel/docs/provenance/GRINNELLI_V2_1_PERFORMANCE_REFERENCE.md)
describes the adaptation and its limitations. No upstream aircraft models,
textures, audio, or compiled binaries are bundled in this source tree.

## Purchased visual content

- SytaPastel: Northrop YF-23 Black Widow II, CGTrader product 2046482.

These assets retain their external licenses. They are separate from the GPL
software and absent from the Git source tree. See
[LICENSE-ASSETS.md](LICENSE-ASSETS.md) for the applicable asset terms.

The exterior has no separate cockpit insert. The Hornet supplies the pilot cockpit
from the user's installation. The removed F-35 insert and its textures are not shipped.
The ten liveries derive from the purchased YF-23 paint source. Exact file records
are in [the livery manifest](config/licensing/liveries-v1.4.json).

## Eagle Dynamics / DCS World

The runtime requires DCS World and an installed, activated DCS: F/A-18C Hornet.
The native bridge uses the user's installed Hornet, and builds require the user's
installed SDK headers. The small test stub is project-authored and is not the
real SDK.

The current aircraft connects its weapons and radar upgrades in memory.
Current installs change no DCS game files. Setup removes verified September 17/18
game-file changes during an upgrade. Manual users first run a full DCS repair.
No Eagle Dynamics scripts or binaries are included in the download.

THIS MATERIAL IS NOT MADE OR SUPPORTED BY EAGLE DYNAMICS SA.

## Installer runtime

The Windows EXE bundles Python 3.12, Tcl/Tk, and the PyInstaller bootloader.
Their licences are included in `LICENSES` here and in the installed `F-23B-docs/LICENSES` folder.
The ZIP contains the same documentation and installation files.
