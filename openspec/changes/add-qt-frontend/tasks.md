# Tasks

## 1. Build & upstream integration

- [x] 1.1 Add SameBoy submodule at `third_party/SameBoy` and the flake-parts `flake.nix` dev shell; verify `git submodule status` shows a pinned commit (pinned at 213a12c, v1.0.3-5)
- [x] 1.2 Write `cmake/SameBoyCore.cmake` (glob `Core/*.c`, version from `version.mk`, upstream flags); verify `sameboy_core` builds warning-clean
- [x] 1.3 Write `cmake/SameBoyAssets.cmake` (boot ROMs via upstream Makefile or `SAMEBOY_BOOTROMS_DIR`, shaders, `registers.sym`); verify all 7 boot ROM `.bin` files and the 16 shader files land in the build dir and the submodule stays clean
- [x] 1.4 Top-level `CMakeLists.txt` with Qt 6 + SDL3, optional camera, install rules; verify `cmake --build` produces `sameboy-qt`
- [x] 1.5 `scripts/sync-sameboy.sh` (tag/master/ref, rebuild, commit range, header diff); verify a dry run against the current pin (also exercised against v1.0.2, which correctly surfaced the missing battery-dirty API as a compile error)
- [x] 1.6 CI workflow: build on push plus a weekly build against upstream `master`; verify the YAML parses and lists both jobs
- [x] 1.8 Desktop entry, upstream FreeDesktop icons (all sizes) and MIME info installed from the submodule; verify `cmake --install` output passes `desktop-file-validate` (also enforced by the flake's installCheckPhase)
- [ ] 1.7 Verify `nix build` / `nix run` of the flake package (needs a Nix-enabled environment; the flake was authored without being able to evaluate it here)

## 2. Settings

- [x] 2.1 `Settings` wrapper with `observe()` and change signals, and defaults ported from `GBApp.m` (keys, values, themes); verify defaults via `--dump-settings` and the `settingsDefaults` test

## 3. Emulation session core

- [x] 3.1 `EmulatorSession`: init, callbacks, emulation thread, `performAtomic`, start/stop, model mapping, auto model; verify a ROM runs headless (`--screenshot`, dmg-acid2 and cgb-acid2 render correctly)
- [x] 3.2 Boot ROM lookup with custom folder and fallbacks; verify the DMG boot animation runs
- [x] 3.3 Battery save timer, save paths, `.gbcart` instances, ISX `.ram`; verify `.sav` is written after closing (`batterySave` test)
- [x] 3.4 Reset/quick reset/reload/hot swap/ROM modification save, and file mtime check; verify manually via menus (reset/model paths covered by `explicitModelAndBorder`; ROM modification tracking by `memoryModel`)
- [x] 3.5 Live-applied settings observers; verify border mode changes on a running session (`explicitModelAndBorder` test)
- [ ] 3.6 Alarm notification scheduling; verify with a ROM that uses alarms (implemented; not verified, no alarm-capable ROM available)

## 4. Display

- [x] 4.1 GL renderer (master shader + filter), triple buffering, frame blending, aspect/integer scaling; verify every filter compiles (all 15 filters rendered under Xvfb/Mesa with no GLSL errors)
- [x] 4.2 QPainter fallback when GL fails; verify with `SAMEBOY_QT_SOFTWARE_RENDERER=1` (output identical to the GL path)
- [x] 4.3 OSD overlay with fade; verify the reset message appears (GL and software paths)
- [x] 4.4 Border mode switching and min window size, window size increase/decrease/zoom, fullscreen and mouse hiding; verify sizes step by integer factors
- [x] 4.5 Screenshots (save, save as, copy, filtered option); verify PNG dimensions of 160×144 raw (`runsAndPicksModel` test)

## 5. Input

- [x] 5.1 Keyboard bindings for 4 players with rapid A/B, turbo, rewind and slow-motion; verify each action in a running game (`keyboardInput`, `rapidFire` tests)
- [ ] 5.2 `GamepadManager` (SDL3): hotplug, default mapping, instance/name mappings, configure wizard, analog speed, faux analog, sensors, rumble, player LEDs, multiplayer assignment, background option, hotkeys; verify with a connected controller (implemented; needs a physical controller to verify)
- [ ] 5.3 MBC7 mouse controls; verify tilt values in the debugger (implemented; needs an MBC7 ROM to verify)
- [ ] 5.4 Workboy key mapping; verify with the Workboy connected (connection covered by `accessories` test; typing not yet verified)

## 6. Audio

- [ ] 6.1 `AudioOutput` with SDL3 pull stream, volume, mute persistence and pause silence; verify audible playback without crackle (implemented and exercised with SDL's dummy driver; audible playback not verified)
- [x] 6.2 Channel mute menu and audio recording (AIFF/WAV/raw); verify the WAV header and duration (`audioRecording` test)
- [x] 6.3 GBS player widget with tracks, prev/next wrap, play/pause and visualizer; verify with a `.gbs` file

## 7. Main window & menus

- [x] 7.1 `MainWindow` menus matching `MainMenu.xib` (File, Edit, Emulation, Cheats, Connect, Develop, Window, Help) with shortcuts and checkmark validation; verify each item's enabled/checked state
- [x] 7.2 Save states (10 slots, legacy fallback, drag & drop); verify save/load round-trip (`saveStateRoundTrip`, `legacyStateFallback` tests)
- [x] 7.3 Open/recent/drag-drop ROMs, multi-window, command line args, and the About dialog with the upstream version; verify opening ROMs from the command line

- [x] 7.4 Idle `WelcomeWindow` with the palette-tinted SDL logo, shown at launch without files and after closing the last game; verify both paths and palette tinting under Xvfb

## 8. Preferences

- [x] 8.1 `PreferencesDialog` Emulation/Video/Audio/Controls tabs bound to settings; verify every tab renders with Cocoa defaults
- [x] 8.3 Replace the Edit menu with an ares-style Settings menu (one entry per Preferences tab + Preferences…) shared by game and idle windows; verify Settings → Video… opens Preferences on the Video tab
- [ ] 8.2 `PaletteEditorDialog` with themes, auto-color generation, manual mode, `.sbp` import/export and restore defaults; verify export→import gives identical parameters (implemented; round-trip not yet verified, and theme selection is covered by `customPalette`)

## 9. Developer tools

- [x] 9.1 Debugger console: output with attributes, input with history and completion, buttons, side view, CPU graph, developer mode, break, help; verify `registers`, `step` and `continue` (`debuggerCommand` test + visual check of break/side view)
- [x] 9.2 Memory viewer: `HexView`, spaces/banks, go-to, editing, status description, 250 ms refresh; verify editing (`memoryModel` test + visual check)
- [x] 9.3 VRAM viewer: tileset, tilemap, objects and palettes tabs with hover status, grid and scroll rect; verify against a running game (cgb-acid2)

## 10. Cheats & accessories

- [x] 10.1 Cheats window (list, edit, import, enable toggle, persistence); verify a GameShark import applies (`cheats` test + visual check)
- [x] 10.2 Cheat search window; verify it renders and searches (visual check; narrowing sequence not yet exercised against a game)
- [ ] 10.3 Printer window (feed, spinner, save PNG, print), Workboy connect, and the Connect menu; verify with a printer-capable ROM (connect/disconnect covered by `accessories`; printing needs a printer ROM)
- [x] 10.4 Link cable & infrared between sessions; verify two linked sessions run and disconnect on close (`linkCable` test)
- [ ] 10.5 Game Boy Camera via Qt Multimedia with black fallback; verify with no camera present (implemented; needs a Game Boy Camera ROM)

## 11. Documentation & verification

- [x] 11.1 README (build, Nix, syncing upstream, feature parity table vs Cocoa); verify it covers the build commands
- [x] 11.2 Full build from a clean tree and a smoke run on a test ROM; verify no warnings in our sources and the submodule is clean
