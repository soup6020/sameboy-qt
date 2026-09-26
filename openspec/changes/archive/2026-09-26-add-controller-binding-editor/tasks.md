# Tasks

## 1. Mapping model

- [x] 1.1 Add `GamepadManager` helpers `mappingForEditing`, `setMapping`, `resetMapping` and `inputDisplayName` (SDL button labels, d-pad/shoulder/trigger/stick names); verify with unit tests that seeding from defaults, moving an input between actions and reset behave as specified

## 2. Controls tab

- [x] 2.1 Rebuild the Controls table as Action | Keyboard | Controller with all 15 actions, "—" for unbindable keyboard cells, a controller selector and "Reset to Defaults"; verify the tab renders under Xvfb with and without a controller
- [x] 2.2 Per-cell capture: double-click to bind (key or controller input), Esc to cancel, Delete / context menu to clear, trigger binding records `AnalogTurbo`/`AnalogUnderclock`; verify via `controllerMappingModel` and the SDL virtual-gamepad test `virtualControllerBinding`
- [x] 2.3 Wizard integration: highlight the row being captured and refresh live; verify with the SDL virtual-gamepad test (`virtualControllerBinding`)

- [x] 2.4 Make each Preferences tab scroll instead of squashing when forced small (tiling WMs); verify with `preferencesScrollWhenSmall`

## 3. Verification

- [x] 3.1 Physical controller check by the user: see bindings after the wizard, rebind one button, clear, reset
- [x] 3.2 Build warning-free, `scripts/lint.sh` clean, tests pass, `openspec validate --strict` passes
