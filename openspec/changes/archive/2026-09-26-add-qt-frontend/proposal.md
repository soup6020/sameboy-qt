# Proposal

## Why

SameBoy's most complete frontend is the macOS Cocoa app. It has a debugger console, memory and VRAM viewers, cheats, a printer feed, a GBS player, palette themes and link-cable multiplayer between windows. On Linux and Windows the only upstream option is the SDL frontend, which has a much smaller feature set. A Qt 6 frontend can bring Cocoa-level functionality to every desktop platform. It must stay cheap to keep in step with upstream, because SameBoy's core changes often.

## What Changes

- Add a new Qt 6 / C++ desktop application (`sameboy-qt`) that embeds the SameBoy core.
- Vendor upstream SameBoy as an **unmodified** git submodule and compile the core, boot ROMs, shaders and symbol files directly from it. Add a sync script and CI check so upstream updates take one command.
- Recreate the Cocoa frontend's user-facing features in Qt:
  - Document-per-window emulation sessions, with model selection, reset and pause.
  - Save states, screenshots, audio recording and battery saves.
  - GPU shader filters, color correction, frame blending, borders, the OSD and palette themes.
  - Keyboard and gamepad input with rapid-fire, turbo, rewind and slow-motion, hotkeys, rumble and motion controls.
  - A Preferences window matching the Cocoa tabs.
  - A developer-mode debugger console, memory viewer and VRAM viewer.
  - A cheats editor and cheat search.
  - Game Boy Printer, Workboy, Game Boy Camera and link cable/infrared between two open windows.
  - A GBS music player.
- Replace macOS-only mechanisms with portable equivalents:
  - NSUserDefaults → QSettings, keeping the same key names.
  - Metal/NSOpenGL → QOpenGLWidget running upstream's GLSL shaders.
  - JoyKit → SDL3 gamepads.
  - AVFoundation → Qt Multimedia camera.
  - NSUserNotification alarms → desktop notifications via the system tray.
- Out of scope (Cocoa features with no portable equivalent or that are Apple-specific): the Sparkle-style self-updater, Joy-Con pairing UI, QuickLook plugin and HexFiend-specific UI polish.

## Capabilities

### New Capabilities
- `upstream-core-sync`: Vendoring, building and updating the unmodified SameBoy core and its assets (boot ROMs, shaders, symbols) from the upstream repository.
- `emulation-session`: Opening ROM, ISX, GBS and cartridge-instance files, one window per session, lifecycle (run, pause, reset, quick reset), model selection, boot ROMs, battery saves, ROM reloading/hot-swapping/modification saving and alarms.
- `display`: Rendering the emulated screen with filters, color correction, frame blending, palettes, borders, aspect/integer scaling, OSD, window sizing, fullscreen, layer toggles and screenshots.
- `input`: Keyboard and gamepad mapping for up to four players, special buttons (rapid A/B, turbo, rewind, slow motion, hotkeys), analog speed control, motion controls, rumble and Workboy typing.
- `audio`: Audio output, volume/mute, high-pass and interference settings, per-channel muting, audio recording and the GBS player.
- `save-states`: Ten save-state slots per game, loading by drag-and-drop, and legacy-slot fallback.
- `preferences`: Persistent, live-applied settings UI mirroring the Cocoa Preferences tabs, including the monochrome palette theme editor.
- `debugger`: Developer mode, the debugger console with side view, stepping controls, CPU-usage meter, history and completion.
- `memory-viewer`: Hex view/editor over Game Boy address spaces with bank selection, go-to-expression and address descriptions.
- `vram-viewer`: Tileset, tilemap, object (OAM) and palette inspection.
- `cheats`: Cheat list editing, GameShark/Game Genie import, persistence and RAM cheat search.
- `accessories`: Serial/IR accessories: Game Boy Printer (feed window, save, print), Workboy, link cable and infrared between sessions, and the Game Boy Camera.

### Modified Capabilities
<!-- None: this is a greenfield project with no existing specs. -->

## Impact

- New code under `src/`, `resources/`, `cmake/`, `scripts/`, plus `CMakeLists.txt`, `flake.nix` (flake-parts) and CI workflows.
- New dependencies:
  - Qt 6 (Widgets, OpenGL, OpenGLWidgets, Multimedia, PrintSupport)
  - SDL3
  - rgbds at build time (optional when prebuilt boot ROMs are supplied)
- Upstream SameBoy is consumed read-only from `third_party/SameBoy`. The core's public API (`Core/gb.h`) is the integration surface, so upstream API changes show up as compile errors in `src/core/`.
