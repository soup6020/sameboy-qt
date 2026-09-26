# Spec Delta

## Purpose

Keeps the SameBoy core, boot ROMs, shaders and debugger symbols sourced from the unmodified upstream repository, so that tracking upstream releases is a mechanical, low-risk operation.

## ADDED Requirements

### Requirement: Upstream vendored unmodified
The project SHALL consume upstream SameBoy from a pinned git submodule at `third_party/SameBoy` and SHALL NOT require any modification of files inside it.

#### Scenario: Clean submodule after build
- **WHEN** the project is configured and built from a fresh clone
- **THEN** `git -C third_party/SameBoy status --porcelain` reports no changes

### Requirement: Core sources discovered automatically
The build SHALL compile every `Core/*.c` file from the submodule without a hand-maintained source list, and SHALL derive the version string from upstream `version.mk`.

#### Scenario: Upstream adds a core source file
- **WHEN** upstream adds a new `Core/foo.c` and the submodule is updated
- **THEN** a rebuild compiles `Core/foo.c` without any build-script edit

#### Scenario: Version shown matches upstream
- **WHEN** the submodule's `version.mk` declares `VERSION := X.Y.Z`
- **THEN** the About dialog and the reset OSD message show "SameBoy vX.Y.Z"

### Requirement: Assets built from upstream
The build SHALL assemble the open-source boot ROMs from upstream `BootROMs/` using RGBDS when available. It SHALL bundle upstream `Shaders/*.fsh`, `Misc/registers.sym` and `SDL/background.bmp`. It SHALL let the packager supply a directory of prebuilt boot ROMs instead of RGBDS.

#### Scenario: Boot ROMs assembled from source
- **WHEN** RGBDS is installed and no prebuilt directory is given
- **THEN** the build produces `dmg_boot.bin`, `mgb_boot.bin`, `cgb0_boot.bin`, `cgb_boot.bin`, `agb_boot.bin`, `sgb_boot.bin` and `sgb2_boot.bin` from the submodule sources

#### Scenario: Prebuilt boot ROMs
- **WHEN** the build is configured with a prebuilt boot ROM directory
- **THEN** those files are bundled and RGBDS is not required

#### Scenario: No boot ROM source available
- **WHEN** neither RGBDS nor a prebuilt directory is available
- **THEN** configuration fails with a message naming both options

### Requirement: Desktop integration from upstream assets
On FreeDesktop platforms, installation SHALL provide:
- a valid `sameboy-qt.desktop` entry;
- upstream's `FreeDesktop/AppIcon` icons at every provided size, installed as `sameboy-qt`;
- upstream's `FreeDesktop/sameboy.xml` MIME definitions, installed as `sameboy-qt.xml`.

Installed file names SHALL NOT collide with an installation of the upstream SDL frontend. The Nix package SHALL include these files.

#### Scenario: Installed package appears in application launchers
- **WHEN** the package is installed (e.g. `nix build` or `cmake --install`)
- **THEN** `share/applications/sameboy-qt.desktop` passes `desktop-file-validate` and references the installed `sameboy-qt` icon

### Requirement: One-command upstream sync
The repository SHALL provide a script that moves the submodule to a given upstream ref (default: latest release tag, or `master` on request). The script SHALL rebuild the project and report upstream commits and changed `Core` public headers since the previous pin.

#### Scenario: Sync to latest release
- **WHEN** a maintainer runs the sync script with no arguments
- **THEN** the submodule is checked out at the newest upstream `v*` tag, the project builds, and a summary lists the upstream commit range and changed public headers

### Requirement: Automated drift detection
Continuous integration SHALL periodically build the project against upstream `master`. It SHALL flag failures, so upstream API breaks are detected before a sync.

#### Scenario: Upstream API change breaks the build
- **WHEN** the scheduled CI job builds against upstream `master` and compilation fails
- **THEN** the job fails and reports the upstream commit that was tested
