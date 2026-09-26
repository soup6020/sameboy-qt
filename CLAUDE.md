# sameboy-qt: notes for future sessions

A Qt 6 / C++20 frontend for SameBoy whose goal is feature parity with upstream's macOS Cocoa frontend. `README.md` covers features, build and sync from a user's point of view. This file covers what you need to work on the code.

## Ground rules

- **Never modify `third_party/SameBoy`.** It is the unmodified upstream submodule, and the build must leave it pristine (`git -C third_party/SameBoy status --porcelain` stays empty). All glue lives in `src/`, `cmake/` and `resources/`.
- **Upstream syncs go through `scripts/sync-sameboy.sh [ref|master]`.** It moves the submodule and the flake's `sameboy` input together, rebuilds, tests, and prints what changed upstream. If the pin moves, update the `sameboy` URL in `flake.nix` too.
- **Port from Cocoa rather than inventing behavior.** Classes deliberately mirror Cocoa files, so read those first:

  | Qt code | Cocoa source |
  |---|---|
  | `EmulatorSession` | `Document.m` |
  | `InputController` | `GBView.m` |
  | `Settings` | `GBApp.m` |
  | `MemoryModel` | `GBMemoryByteArray.m` |

  Comments name the Cocoa method being ported. Keep the method order and names comparable so upstream diffs can be ported.
- **Settings keys and values mirror Cocoa's NSUserDefaults.** Examples: `GBFilter`, `GBColorCorrection`, `GBEmulatedModel`, `JoyKit*Mapping`. Defaults live in `Settings::registerDefaults()`. Palette themes are transcribed in `PaletteThemes.cpp`.
- **Use OpenSpec for behavior changes.** Specs live in `openspec/` (see "OpenSpec" below).
- **Nix:** use flake-parts (the user's global preference). Don't add flake-utils.
- **Git:** don't commit unless asked. The user reviews staged changes.

## Build & test

In the sandbox, `nix` itself is blocked (permission denied on `~/.local/share/nix`). The user generated `.dev-env.sh` (gitignored) with `nix print-dev-env`. Source it with **bash**, not zsh:

```sh
bash -c 'source .dev-env.sh >/dev/null 2>&1; cmake -S . -B build -G Ninja && cmake --build build'
bash -c 'source .dev-env.sh >/dev/null 2>&1; cd build && QT_QPA_PLATFORM=offscreen SDL_AUDIO_DRIVER=dummy ctest --output-on-failure'
```

If `.dev-env.sh` is missing or stale (for example after `flake.nix` changes), ask the user to run this **outside** Claude Code:

```sh
nix print-dev-env > .dev-env.sh
```

A `!` command runs inside the sandbox and fails.

Toolchain: Qt 6.11, SDL3, rgbds (boot ROMs and the test ROM), GCC. The build must stay warning-free in `src/` and `tests/` (`-Wall -Wextra`).

**Formatting and linting** (clang-tools from the dev shell; config in `.clang-format`, `.clang-tidy` and `.clangd`):
- `scripts/lint.sh format` reformats our sources. Run it before finishing any change.
- `scripts/lint.sh` checks formatting and runs clang-tidy with warnings as errors, which CI enforces. It needs `build/compile_commands.json` (CMake exports it), and a full run takes several minutes.
- Use `scripts/lint.sh tidy <files>` to check only what you touched.
- `third_party/` and moc output are never checked.
- To silence a clang-tidy finding, fix the code or narrow the check in `.clang-tidy` with a reason. Don't scatter `NOLINT`.

- **Tests:** `tests/tst_sameboy.cpp` (QtTest) runs `tests/testrom.asm`, which is assembled at build time. It counts frames at `$C000`, mirrors the joypad to `$C001` and writes `$42` to SRAM `$A000`.
  - SameBoy randomizes RAM at power-on, so tests must wait via `waitForGame()`: PC is in the ROM's main loop and the SRAM marker is written.
  - Tests set `XDG_CONFIG_HOME` to a temp dir. Always isolate settings like this; never touch the user's real `~/.config/sameboy-qt`.
- **Headless frame check:**

  ```sh
  ./build/sameboy-qt --screenshot out.png --duration 3000 rom.gb
  ```
- **Real UI check (GL included):** start `Xvfb :99 -screen 0 1280x1024x24 +extension GLX &`, then run with `DISPLAY=:99 QT_QPA_PLATFORM=xcb LIBGL_ALWAYS_SOFTWARE=1 SDL_AUDIO_DRIVER=dummy XDG_CONFIG_HOME=<scratch>`. Developer hooks in `main.cpp`:
  - `SAMEBOY_QT_TRIGGER="Show Memory;Video"` triggers menu items by title (with `&` and `…` stripped) after 1 s.
  - `SAMEBOY_QT_GRAB=<prefix>:<ms>` saves every visible top-level window, and every tab of a `QTabWidget`, as PNGs, then quits.
  - `SAMEBOY_QT_SOFTWARE_RENDERER=1` forces the non-GL renderer (the tests set it, since the offscreen platform has no OpenGL).
  - `SAMEBOY_QT_VIRTUAL_GAMEPAD=1` attaches an SDL virtual gamepad, to check controller UI without hardware. Tests can drive one directly (see `virtualControllerBinding`).

  Don't screenshot the user's real desktop. Kill Xvfb when done. `pkill` returns 144; append `; true` if it ends a command chain.
- **Test ROMs:** dmg-acid2 and cgb-acid2 (MIT, from GitHub releases) are good visual references. Download them into the scratchpad, not the repo.
- **Stale moc:** if the link fails with "undefined reference to vtable/staticMetaObject" after adding a `Q_OBJECT` header, delete `build/sameboy-qt*_autogen` and re-run cmake.

## Architecture

```
cmake/SameBoyCore.cmake    globs Core/*.c (-DGB_INTERNAL, upstream warning flags), version from version.mk
cmake/SameBoyAssets.cmake  boot ROMs via upstream `make bootroms` (BIN/OBJ/PB12_COMPRESS redirected into build/),
                           or -DSAMEBOY_BOOTROMS_DIR; stages Shaders/, registers.sym, SDL/background.bmp,
                           LICENSE into build/share/sameboy-qt (the build tree is runnable in place)
src/core/      EmulatorSession, AudioOutput (SDL3 pull stream), CameraProvider (Qt Multimedia),
               ResourceLocator (finds share/sameboy-qt), Models.h (Cocoa-side enums)
src/settings/  Settings (QSettings, observe()), PaletteThemes
src/input/     GamepadManager (SDL3, singleton), InputController (per window)
src/ui/        MainWindow (menus = MainMenu.xib), AppController (open/recent/prefs/alarms/idle window),
               WelcomeWindow (idle, palette-tinted SDL logo), ScreenWidget + GLScreenRenderer, OSDOverlay,
               PreferencesDialog, PaletteEditorDialog, DebuggerConsole/ConsoleInput/CpuGraph,
               MemoryViewer/HexView/MemoryModel, VramViewer, CheatsWindow, CheatSearchWindow,
               PrinterWindow, GbsPlayerWidget
```

Everything except `main.cpp` builds into `sameboy-qt-lib`, which the app and the tests both link. Sources are globbed (`CONFIGURE_DEPENDS`).

### Threading (the most important invariant)

Each `EmulatorSession` runs `GB_run` on its own `std::thread`, as in Document.m.

- **Mutating core state from the GUI:** use `performAtomic(block)`. It runs between `GB_run` steps, or inline when the session is stopped or at the debugger prompt.
- **Capturing log output:** `captureOutput(block)`, for example around `GB_debugger_evaluate`.
- **Core callbacks run on the emulation thread.** They must never block on the GUI thread, because `stop()` joins the thread. Use `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`.
- **Focus / inactivity:** `MainWindow::isSessionFocused()` treats the window, its tool windows (tagged by `adoptToolWindow`, i.e. the `sameboyOwner` property), the link partner and app dialogs as focused. Give new tool windows `adoptToolWindow()`.
- **Emulation-thread code must not read `Settings` (QSettings).** Cache the values in atomics through `Settings::observe` instead (see `m_borderMode`).
- **Frame buffers:** three buffers, always 256×224, so border changes never reallocate. `InputController::frameHook` runs on the emulation thread each frame; it drives rapid fire and slow-motion, as GBView's `-flip` does.
- **Link cable:** the master session runs both cores on its thread, and the slave's start/stop delegates to the master.
- **Debugger:** input blocks the emulation thread on a condition variable. The side view re-runs its commands on the emulation thread whenever it stops.

### Rendering gotchas (learned the hard way)

- Upstream shaders are `#version 150`, so the default surface format is 3.2 core.
- `GLScreenRenderer::paintGL` must disable `GL_DEPTH_TEST`, scissor and cull. Qt leaves depth testing on, which silently drops the second (OSD) pass.
- **Don't use QPainter on the core-profile GL context.** It draws nothing, and child widgets over a `QOpenGLWidget` aren't composited either. The OSD is therefore rasterized into a `QImage` and blended as a texture.
- **Software fallback:** if shaders fail, ScreenWidget deletes the GL child and paints with the raster QPainter engine.
- **OSD timing matches GBOSDView exactly:** about 0.6 s for single-line messages and about 1.6 s for multi-line ones. Grab within that window when testing.

## Conventions

- Match the surrounding style: 4-space indent, `m_` members, `QStringLiteral`, braces on their own line for functions, and one comment per non-obvious block explaining *why* (often pointing at the Cocoa equivalent).
- **Names and paths:**
  - app and desktop IDs: `sameboy-qt`;
  - launcher name: `Sameboy-qt`;
  - window titles and About box: "SameBoy";
  - settings: `~/.config/sameboy-qt/sameboy-qt.conf`, with a one-time migration from `~/.config/SameBoy/SameBoy-Qt.conf`;
  - saves, states and cheats: next to the ROM, as in Cocoa.
- **Menus:** Cocoa's layout, except that Edit is replaced by an ares-style Settings menu (one entry per Preferences tab plus Preferences…). It is built by `AppController::addSettingsMenu`, which is shared by `MainWindow` and `WelcomeWindow`.
- **Shortcuts:**
  - Ctrl+digit is save states and Ctrl+Shift+digit is load states;
  - Alt+1..4 toggles the audio channels;
  - Break Debugger is Ctrl+C (⌃C on macOS).

  Check `MainWindow::buildMenus` before adding shortcuts.
- **Installed files:** Linux installs a desktop entry, upstream's `FreeDesktop/AppIcon` sizes as `sameboy-qt`, and upstream MIME XML as `sameboy-qt.xml`. Cartridge MIME icons are left out to avoid file collisions with nixpkgs' `sameboy`. The flake's `installCheckPhase` validates these files.

## OpenSpec

- Specs live in `openspec/`, and the project context is in `openspec/config.yaml`. Slash commands are `/opsx:propose`, `/opsx:apply` and `/opsx:archive`, and there are skills in `.claude/skills/`.
- The baseline specs live in `openspec/specs/` (12 capabilities, from the archived `add-qt-frontend`).
- `verify-hardware-features` (tasks only, `skip_specs`) tracks checks that need ROMs or peripherals the sandbox lacks: alarms, MBC7, Workboy typing, `.sbp` round trip, printer, camera.
- **Making changes:** new behavior gets its own change (`openspec new change <name>`) with delta specs against `openspec/specs/`. Validate with `openspec validate --all --strict`, and archive once the user has confirmed any manual checks.
- **Avoid parallel conflicts:** two open changes shouldn't both MODIFY the same requirement. Archiving the first would make the second's copy stale.
