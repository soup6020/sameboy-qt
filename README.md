# sameboy-qt

A Qt 6 desktop frontend for [SameBoy](https://github.com/LIJI32/SameBoy), the accurate Game Boy / Game Boy Color emulator. The goal is to match the macOS Cocoa frontend's functionality on Linux, Windows and macOS.

The SameBoy core is used **unmodified**. It is a git submodule at `third_party/SameBoy`, compiled directly from upstream sources, so following upstream is a one-command operation.

## Building

### With Nix (recommended)

```sh
nix develop            # Qt 6, SDL3, RGBDS, CMake, Ninja
cmake -S . -B build -G Ninja
cmake --build build
./build/sameboy-qt path/to/game.gb
```

`nix build` produces a wrapped package, and `nix run . -- game.gb` runs it. The flake uses flake-parts.

### Manually

You need:
- CMake ≥ 3.21 and a C11/C++20 compiler
- Qt 6.4+: Widgets, OpenGL, OpenGLWidgets, PrintSupport, and optionally Multimedia for the Game Boy Camera
- SDL3
- [RGBDS](https://rgbds.gbdev.io) and `make`, to assemble SameBoy's open-source boot ROMs from upstream sources

```sh
git clone --recursive <this repo>
cmake -S . -B build -G Ninja && cmake --build build
```

If you can't install RGBDS, pass `-DSAMEBOY_BOOTROMS_DIR=/path/with/dmg_boot.bin…` with prebuilt boot ROMs.

Other options:
- `-DSAMEBOY_QT_CAMERA=OFF` builds without Qt Multimedia.
- `-DSAMEBOY_QT_BUILD_TESTS=OFF` skips the tests.

On Linux, `cmake --install` (and the Nix package) also installs a desktop entry, upstream's app icon in every size, and MIME definitions for `.gb`/`.gbc`/`.isx`.

The build tree is runnable in place. `cmake --install` puts assets in `share/sameboy-qt`, and `SAMEBOY_QT_DATA_DIR` overrides the asset location.

## Keeping up with upstream

```sh
scripts/sync-sameboy.sh           # newest upstream release tag
scripts/sync-sameboy.sh master    # upstream master
scripts/sync-sameboy.sh <ref>     # any tag/branch/commit
```

The script:
- moves the submodule;
- updates the flake's matching `sameboy` input;
- rebuilds and runs the tests;
- checks the submodule is still pristine;
- prints what changed upstream: commits, public `Core/*.h` headers, Cocoa frontend files worth porting, and consumed assets.

It also flags changes to `Cocoa/GBApp.m` defaults and themes, which are transcribed into `src/settings`.

Why this stays cheap:
- `Core/*.c` is globbed, and the version comes from `version.mk`.
- Boot ROMs are built by upstream's own Makefile, with output redirected into the build directory.
- Shaders (`Shaders/*.fsh`), `Misc/registers.sym` and the SDL idle-screen logo (`SDL/background.bmp`) are used verbatim.
- Only `src/core/EmulatorSession.cpp` and the developer tool windows call the core API. An upstream API change shows up as a compile error there.
- The *Upstream drift* GitHub workflow builds and tests against upstream `master` weekly.

## Feature parity with the Cocoa frontend

| Cocoa feature | Qt frontend |
|---|---|
| Document-per-window sessions, Open Recent, drag & drop | ✅ |
| Idle state | ✅ Idle window with the SDL frontend's logo, tinted with the current palette, instead of an open panel |
| Reset / Quick Reset / Reload ROM / Pause | ✅ |
| Emulated model menu, automatic model, per-family hardware revisions | ✅ |
| Built-in or custom boot ROM folder | ✅ |
| Battery saves (+ `.ram` for ISX), `.gbcart` cartridge instances | ✅ |
| Hot swap cartridge, Save ROM Modifications (memory-viewer edits) | ✅ |
| Reload when the ROM changes on disk | ✅ (on window activation) |
| Save states (10 slots, legacy `.snN`, drag & drop) | ✅ |
| Screenshots (save, save as, copy, filtered) | ✅ |
| All upstream shaders, frame blending, color correction, light temperature | ✅ (OpenGL 3.2; QPainter fallback) |
| Monochrome palettes and palette theme editor (`.sbp` import/export) | ✅ |
| Borders, keep aspect ratio, integer scaling, OSD, window size stepping, full screen | ✅ |
| Keyboard mapping for 4 players, rapid A/B, turbo, rewind, slow motion | ✅ |
| Controllers: mapping wizard, hotkeys, analog speed, faux analog, rumble, motion (MBC7), multiplayer assignment, background input | ✅ via SDL3 |
| Mouse motion controls for MBC7 | ✅ |
| Audio, volume, mute, high-pass filter, interference, channel muting | ✅ via SDL3 |
| Audio recording (AIFF / WAV / raw) | ✅ |
| GBS player with visualizer | ✅ |
| Debugger console: prompt, history, reverse search, completion, stepping buttons, side view, CPU meter | ✅ |
| Memory viewer / hex editor with banks and go-to | ✅ |
| VRAM viewer (tileset, tilemap, objects, palettes) | ✅ |
| Cheats window and cheat search | ✅ |
| Game Boy Printer (feed window, save, print), Workboy | ✅ |
| Link cable & infrared between two open windows | ✅ |
| Game Boy Camera | ✅ via Qt Multimedia |
| Controller binding editor (keyboard and controller side by side, per-button rebind/clear/reset) | ✅ Beyond Cocoa, which only has the configuration wizard |
| Pause / mute when the window is inactive (both optional, off by default) | ✅ Beyond Cocoa |
| Alarm notifications | ⚠️ Only while the app keeps running (system tray message) |
| Joy-Con pairing UI, auto-updater, QuickLook | ❌ Apple-specific / out of scope |

Differences worth knowing:
- Keyboard bindings are stored as Qt key codes rather than Mac virtual key codes.
- Preferences are reached from a **Settings** menu (like ares), with one entry per tab (Emulation, Video, Audio, Controls) plus Preferences… (Ctrl+,), instead of Cocoa's app-menu item.
- Menu shortcuts use Ctrl where Cocoa uses ⌘. On macOS Qt maps them back to ⌘.
- Break Debugger is Ctrl+C on Linux/Windows (⌃C on macOS).
- Save ROM Modifications is Ctrl+Alt+Shift+S (⌃⌘S on macOS).
- Settings live in `~/.config/sameboy-qt/sameboy-qt.conf` on Linux (the platform-native store elsewhere). They use the Cocoa key names (`GBFilter`, `GBColorCorrection`, …). As in the Cocoa app, battery saves (`.sav`), save states (`.s1`…`.s10`) and cheats (`.cht`) are stored next to each ROM, or inside a `.gbcart` cartridge instance.

## Formatting and linting

The dev shell includes clang-format, clang-tidy and clangd. Configuration lives in `.clang-format`, `.clang-tidy` and `.clangd` (clangd reads `build/compile_commands.json`).

```sh
scripts/lint.sh format   # reformat src/ and tests/
scripts/lint.sh          # format check + clang-tidy (what CI runs); needs a configured build/
```

## Tests

```sh
cd build && QT_QPA_PLATFORM=offscreen SDL_AUDIO_DRIVER=dummy ctest --output-on-failure
```

The suite (`tests/tst_sameboy.cpp`) runs a tiny RGBDS-built ROM (`tests/testrom.asm`). It covers:
- settings defaults and themes;
- automatic model selection and border switching;
- save states, including legacy slots;
- battery saves;
- keyboard input and rapid fire;
- audio recording;
- cheats;
- the memory model and ROM-modification tracking;
- link cable;
- accessories;
- debugger commands and expression evaluation.

`sameboy-qt --screenshot out.png --duration 3000 game.gb` runs a ROM headlessly and saves the frame.

Developer hooks for scripted UI checks (not user features):
- `SAMEBOY_QT_TRIGGER="Show Console;Show Memory"` triggers menu items after 1 s.
- `SAMEBOY_QT_GRAB=<prefix>:<ms>` saves every visible window (and each tab) as PNGs, then quits.

## Project layout

```
third_party/SameBoy/   upstream, unmodified (submodule)
cmake/                 core + asset build glue
src/core/              EmulatorSession (Document.m's engine), audio, camera, resources
src/settings/          QSettings wrapper with Cocoa key names and defaults, palette themes
src/input/             keyboard/controller routing (GBView.m), SDL3 gamepads
src/ui/                main window/menus and every tool window
tests/                 QtTest suite + test ROM
openspec/              specifications and change history (OpenSpec)
scripts/               sync-sameboy.sh
```

The requirements are specified with [OpenSpec](https://github.com/Fission-AI/OpenSpec) under `openspec/`. Propose behavior changes as OpenSpec changes (`/opsx:propose`).

## License

This frontend is released under the MIT license. SameBoy is © Lior Halphon, MIT licensed (see `third_party/SameBoy/LICENSE`).
