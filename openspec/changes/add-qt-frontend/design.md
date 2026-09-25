# Design

## Context

Upstream's Cocoa frontend (`third_party/SameBoy/Cocoa`) has three layers:
- `Document.m`: one `GB_gameboy_t` per window, plus its emulation thread, audio buffer, the debugger console, memory/VRAM viewers, printer and link cable.
- `GBView`: rendering and input.
- `GBApp`/`GBPreferencesWindow`: defaults and preferences.

The core is plain C11 with a callback-based public API (`Core/gb.h`), so it maps cleanly onto a C++ wrapper. The requirements are in `specs/`, and the motivation and scope are in `proposal.md`.

## Goals / Non-Goals

**Goals:**
- Feature parity with Cocoa for all behavior in `specs/`, on Linux first, with no platform-specific code outside small `#ifdef`s.
- Zero patches to upstream. The only integration surface is the public headers plus the asset folders.
- A structure that lets new upstream Cocoa features be ported file-by-file. Qt classes mirror the Cocoa class boundaries.

**Non-Goals:**
- The auto-updater, Joy-Con pairing/grip UI, QuickLook/thumbnailers, iOS and libretro.
- A Metal renderer. Only OpenGL 3.2 core is supported. On GPUs without it, the app falls back to a QPainter path with nearest/bilinear scaling only.

## Decisions

### Upstream as git submodule, core compiled by CMake glob
- **Choice.** `third_party/SameBoy` is a submodule pinned to a commit. `cmake/SameBoyCore.cmake`:
  - builds a static `sameboy_core` target from `file(GLOB Core/*.c)`, compiled as C11 with `-DGB_INTERNAL` and upstream's warning suppressions;
  - parses `VERSION` from `version.mk` and the copyright year from `LICENSE`.

  The Qt code includes only `Core/gb.h` without `GB_INTERNAL`.
- **Why.** Globbing means upstream file additions need no build edits. Building the core ourselves (instead of `make lib`) avoids a Make dependency and gives native CMake/MSVC support.
- **Alternatives.** A git subtree is harder to diff and invites local patches. Upstream `make lib` works but is Unix-only and needs its toolchain detection.

### Boot ROMs via upstream Makefile, with a prebuilt override
- **Choice.** A CMake custom command runs `make -C third_party/SameBoy bootroms BIN=<build>/sameboy OBJ=<build>/sameboy/obj`. This keeps the submodule tree clean and reuses upstream's rgbds version handling and pb12 compression. `-DSAMEBOY_BOOTROMS_DIR=<dir>` skips the build and copies prebuilt `.bin` files instead.
- **Why.** The boot ROM build steps (rgbgfx → pb12 → rgbasm/rgblink with version-dependent flags) change upstream occasionally. Delegating to upstream's Makefile tracks those changes for free.

### Assets bundled from the submodule at build time
- **Choice.** Shaders, `registers.sym` and boot ROMs are copied next to the executable (`<bin>/../share/sameboy-qt` when installed) and located at runtime through `ResourceLocator`. They are not compiled into a `.qrc`, so they can be overridden.
- **Details.** The GLSL master shader is used verbatim with `{filter}` substitution, exactly as `GBGLShader.m` does. The uniforms are `image`, `previous_image`, `frame_blending_mode`, `output_resolution` and `origin`.

### Threading model mirrors Cocoa
- **Choice.** Each `EmulatorSession` (the equivalent of `Document`) owns a `GB_gameboy_t*` (via `GB_alloc`/`GB_init`) and a `std::thread` running `GB_run` in a loop.
- **Mutations.** State mutations from the UI go through `performAtomic(std::function)`, which posts a block that the emulation thread runs between `GB_run` calls. It runs inline when stopped or when called on the emulation thread. This is the same approach as `performAtomicBlock:`.
- **Callbacks.** Core callbacks run on the emulation thread:
  - vblank: flip the triple buffer, then `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` to repaint.
  - log: append to a locked pending buffer that the GUI flushes every 50 ms.
  - debugger input: blocks on a condition variable fed by the console.
- **Why.** This matches the core's expectations (for example, debugger input blocking the emulation thread) and the Cocoa semantics exactly.
- **Alternatives.** Running in a QTimer on the GUI thread was rejected: debugger input blocks, and turbo and rewind need a free-running loop.

### Link cable: master drives both cores on one thread
The Cocoa master/slave scheme is ported directly: `_linkOffset` interleaving uses multiplication tables built from `GB_get_clock_rate`, and the slave runs in turbo mode with no turbo cap. Sessions are looked up through a `SessionRegistry` singleton, which the Connect menu also uses.

### Rendering: QOpenGLWidget + triple buffer
- **Choice.** `ScreenWidget` (QOpenGLWidget, 3.2 core profile) keeps `GBViewBase`'s three `uint32_t` buffers. With blending on, current/previous/pixels rotate modulo 3; otherwise modulo 2. It reproduces `-[GBView setFrame:]` for aspect ratio and integer scale inside `paintGL` by computing a viewport.
- **Overlay.** The OSD is a separate transparent child `QWidget` painted with QPainter (outline by offset drawing), as in `GBOSDView`.
- **Screenshots.** Filtered screenshots use `grabFramebuffer()`. Raw screenshots copy the current buffer into a `QImage::Format_RGBX8888`, which matches the core's `r | g<<8 | b<<16 | 0xFF<<24` rgb encoding.

### Input: Qt key events + SDL3 gamepads
- **Keyboard.** Bindings are stored as Qt `nativeScanCode` plus `key`. Matching is on the key code, so bindings stay layout-independent where possible. Modifier-only keys (Shift for Slow-Motion) are handled through `keyPress`/`keyRelease` of `Qt::Key_Shift`.
- **Gamepads.** `GamepadManager` polls `SDL_Gamepad` events on a 4 ms QTimer on the GUI thread. It exposes button, axis and sensor events with a stable `uniqueID` (GUID + serial/path) and device name. This mirrors JoyKit's instance and name mapping keys (`JoyKitInstanceMapping`, `JoyKitNameMapping`, `JoyKitDefaultControllers`). Rumble uses `SDL_RumbleGamepad`, player LEDs use `SDL_SetGamepadPlayerIndex`, and accelerometers use `SDL_SetGamepadSensorEnabled`.
- **Why SDL3.** Its gamepad DB and sensor/rumble support give the best cross-platform coverage. RMG uses the same approach.

### Audio: SDL3 audio stream per session
- **Choice.** Each session opens an `SDL_AudioStream` (stereo S16 at 96 kHz; SDL resamples). A pull callback reproduces `Document.m`'s renderer block:
  - it waits on a condition until enough samples arrive (bounded by buffer duration);
  - on underrun it zero-pads;
  - when the backlog exceeds `nFrames + 4800` it skips ahead.
- **Why.** The pull model with waiting gives Cocoa-equivalent latency and pacing. Qt Multimedia's `QAudioSink` push model was considered but has less predictable latency across backends.

### Settings: QSettings with Cocoa key names
- **Choice.** `Settings` wraps `QSettings`. `registerDefaults()` ports `GBApp.m`'s defaults dictionary, including the palette themes. It emits `changed(key)`, and `observe(key, lambda)` mirrors `observeStandardDefaultsKey`. Enum values are stored as the core's integer values, like Cocoa does.
- **Why.** Keeping key names lets future upstream Cocoa changes be ported by grepping the key name.

### Memory viewer: custom hex widget
- **Choice.** HexFiend has no Qt equivalent that supports banked, lazily read memory, so `HexView` is a custom QAbstractScrollArea. It reads through a `MemoryModel` that ports `GBMemoryByteArray`'s copy and insert logic chunked at 0x1000 boundaries. It supports overwrite editing in hex and ASCII columns, and has a status bar using `GB_debugger_describe_address`.

### Camera and printing via Qt Multimedia / PrintSupport
- **Camera.** `CameraProvider` lazily creates a `QCamera` with a `QMediaCaptureSession` and `QVideoSink`. On request it converts the latest frame to grayscale, cover-scales to 130×114 and center-crops 128×112.
- **Printing.** Printing uses `QPrintDialog` with `QPainter` on `QPrinter`.
- **Build flag.** Qt Multimedia is optional (`SAMEBOY_QT_CAMERA`). Without it, the camera yields black frames.

### Alarm notifications
- **Choice.** A `QSystemTrayIcon::showMessage` is fired from a `QTimer` when the app is still open at the scheduled time. It is cancelled when the same ROM starts again.
- **Why.** No portable scheduled OS notification exists, so this is the closest equivalent (see Risks).

### Code layout
```
src/core/      EmulatorSession, SessionRegistry, AudioOutput, CameraProvider, ResourceLocator, Paths
src/settings/  Settings, Defaults, PaletteThemes
src/input/     KeyBindings, GamepadManager, InputController
src/ui/        MainWindow, ScreenWidget, OSDOverlay, ShaderRenderer, PreferencesDialog,
               PaletteEditorDialog, DebuggerConsole, CpuGraph, MemoryViewer, HexView, VramViewer,
               PrinterWindow, CheatsWindow, CheatSearchWindow, GbsPlayerWidget, Visualizer
scripts/       sync-sameboy.sh
cmake/         SameBoyCore.cmake, SameBoyAssets.cmake
```

## Risks / Trade-offs

- **[Risk] Upstream changes the public API or callback signatures.** → Only `src/core/EmulatorSession.cpp` and the viewers touch the core API. The weekly CI job against upstream `master` surfaces breaks early. The sync script prints header diffs.
- **[Risk] Upstream adds files to `Core/` that need platform-specific flags, or moves to a different boot ROM toolchain.** → We delegate boot ROMs to upstream's Makefile. Core flags mirror upstream's `CFLAGS` and are checked by the drift CI.
- **[Risk] The pinned submodule and the flake's view of sources diverge.** → The flake builds from the working tree including submodules (`git+file:...?submodules=1` / `self.submodules = true`). The sync script is the only thing that moves the pin.
- **[Trade-off] Alarms only fire while the app runs**, unlike macOS scheduled notifications. This is documented in the About/README.
- **[Trade-off] Keyboard bindings use Qt key codes**, not physical keycodes as on macOS. Layout changes may alter bindings for letter keys.
- **[Risk] OpenGL 3.2 core may be unavailable in VMs or remote sessions.** → The QPainter fallback renderer is chosen automatically if shader compilation fails.

## Migration Plan

Not applicable (greenfield). Users of the Cocoa app cannot import settings automatically. Palette themes can be moved via `.sbp` export/import, which is format-compatible.
