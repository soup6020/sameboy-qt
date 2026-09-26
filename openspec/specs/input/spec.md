# input Specification

## Purpose
Defines how keyboard, gamepad and mouse input reaches the emulated console, including multiplayer mapping and the special speed-control buttons, matching the Cocoa frontend's controls.

## Requirements

### Requirement: Keyboard mapping
Each of Right, Left, Up, Down, A, B, Select, Start, Rapid A, Rapid B SHALL be bindable to a key for each of Players 1–4. Turbo, Rewind and Slow-Motion SHALL be bindable for Player 1. The defaults SHALL be arrows, X=A, Z=B, Backspace=Select, Return=Start, Space=Turbo, Tab=Rewind, Shift=Slow-Motion. Keys bound more than once SHALL be highlighted in red in the mapping table.

#### Scenario: Rebind A
- **WHEN** the user clicks the A row in Controls and presses "K"
- **THEN** K presses A in all sessions and the setting persists

#### Scenario: Modifier key binding
- **WHEN** the user binds Slow-Motion to Right Shift
- **THEN** holding Right Shift activates slow motion

### Requirement: Rapid fire
While Rapid A or Rapid B is held, the corresponding button SHALL toggle every two frames. The toggling SHALL start pressed.

#### Scenario: Rapid A
- **WHEN** Rapid A is held for 8 frames
- **THEN** A is pressed on frames 1–2 and 5–6 and released on frames 3–4 and 7–8

### Requirement: Speed controls
Holding Turbo SHALL run uncapped or at the turbo cap. Holding Rewind SHALL rewind (disabling turbo, and not available while link-cabled). Holding Slow-Motion SHALL ramp the clock multiplier down by 1/16 per frame to 0.5, and it SHALL ramp back to 1.0 on release. The OSD SHALL show the active mode.

#### Scenario: Slow motion ramp
- **WHEN** Slow-Motion is held for 8 frames
- **THEN** the clock multiplier reaches 0.5

### Requirement: Gamepad support
Connected game controllers SHALL drive the emulator with default mappings:
- D-pad → directions; A/B → A/B; Start or X → Start; Back or Y → Select
- Left shoulder → Turbo; left/right triggers → Rewind; right shoulder → Slow-Motion

A "Configure a controller" wizard SHALL capture a button for each action in sequence, with Skip. It SHALL store per-device-instance and per-device-name mappings, and SHALL record analog axes for Turbo and Slow-Motion when an axis is used. While the wizard runs, the Controls table SHALL highlight the action being captured and show each binding as soon as it is captured.

#### Scenario: Configure controller
- **WHEN** the user runs the wizard and presses buttons for each prompt
- **THEN** the controller uses the new mapping after the wizard ends, and the Controls table shows the new bindings

### Requirement: Analog speed control
When "Analog turbo and slow-motion controls" is enabled, the mapped analog Turbo axis SHALL set the clock multiplier between 1× and 3×. The analog Slow-Motion axis SHALL set it between 1/3× and 1×.

#### Scenario: Half-pressed turbo trigger
- **WHEN** the analog turbo axis reads 0.5
- **THEN** the clock multiplier is 2.45

### Requirement: Multiplayer controller assignment
With more than one player (SGB multiplayer or a link cable), each player SHALL use the preferred controller chosen in Preferences, or the next unassigned controller that sends input. Controllers not assigned to a player SHALL be ignored. In single-player mode every controller SHALL control player 1.

#### Scenario: Two controllers on SGB multiplayer game
- **WHEN** a 2-player SGB game runs and two controllers press buttons
- **THEN** the first controller drives player 1 and the second drives player 2

### Requirement: Background controllers
Controller input SHALL only reach the focused session window. With "Enable controllers while in background" on, it SHALL instead reach the most recently focused session.

#### Scenario: Background disabled
- **WHEN** another application is focused and background controllers are off
- **THEN** controller input does not affect any session

### Requirement: Controller hotkeys
Two bindable controller Hotkeys SHALL each trigger a configurable action: Toggle Pause, Reset, Toggle Mute, Save State to Slot N, or Load State from Slot N (N = 1–10).

#### Scenario: Hotkey saves state
- **WHEN** Hotkey 1 is set to "Save State to Slot 3" and pressed
- **THEN** slot 3 is written for the active session

### Requirement: Motion controls (MBC7)
For games with an accelerometer, the application SHALL supply tilt from the first available source:
- Controller motion sensors, unless "Prefer joysticks over motion controls" is set or no sensor exists.
- Otherwise, the controller's left stick.
- The mouse position relative to the screen, when "Allow mouse controls" is on and the mouse was used last. Mouse click presses A.

#### Scenario: Mouse tilt
- **WHEN** an MBC7 game runs, mouse controls are allowed, and the mouse moves to the right edge
- **THEN** the core receives an X acceleration of -1

### Requirement: Faux analog stick
When "Use joysticks as faux analog controls" is enabled, controller stick movement SHALL drive the core's faux analog input. Pressing a digital direction SHALL switch that player back to digital input.

#### Scenario: Stick used
- **WHEN** faux analog is on and the left stick is tilted halfway right
- **THEN** the core receives faux analog X of 0.5 for that player

### Requirement: Rumble
Core rumble SHALL drive the last-used controller's rumble. Amplitude SHALL be adjusted by the rumble strength setting (`amp^s * s`). The rumble mode (Never / For rumble-enabled Game Paks / Always) SHALL be applied to the core.

#### Scenario: Rumble cart
- **WHEN** a rumble cartridge activates its motor and a controller with rumble is in use
- **THEN** that controller vibrates

### Requirement: Workboy keyboard
When a Workboy is connected, typed characters, function keys F1–F10, arrows, Backspace, Escape and Shift SHALL map to Workboy keys and SHALL NOT act as joypad input.

#### Scenario: Typing on Workboy
- **WHEN** the Workboy is connected and the user types "a"
- **THEN** the Workboy receives key A

### Requirement: Controller binding editor
The Controls tab SHALL show, for one selected controller, the controller inputs bound to every action (Right through Start, Rapid A/B, Turbo, Rewind, Slow-Motion, Hotkey 1, Hotkey 2). Inputs SHALL be labelled with names appropriate to the controller type (for example "A", "Cross", "D-pad Up", "Left Trigger"). Controllers without a custom mapping SHALL show their default bindings.

Individual controller bindings SHALL be editable:
- Double-clicking an action's controller cell SHALL wait for the next input from the selected controller and bind it to that action, replacing the action's previous inputs.
- An input SHALL be bound to at most one action; binding it elsewhere SHALL remove it from its previous action.
- Esc SHALL cancel waiting. Delete, or "Clear Binding" in the cell's context menu, SHALL remove the action's inputs.
- The first edit of a controller without a custom mapping SHALL start from its default mapping.
- Binding Turbo or Slow-Motion to a trigger SHALL also record it as the analog axis for that action.
- "Reset to Defaults" SHALL delete the selected controller's custom mapping.

Edits SHALL be stored the same way as the wizard's (per instance and per device name) and SHALL apply immediately.

#### Scenario: See current bindings
- **WHEN** a controller has been configured with the wizard and the user opens the Controls tab
- **THEN** the Controller column shows each action's bound input by name

#### Scenario: Rebind a single button
- **WHEN** the user double-clicks the Controller cell of B and presses the controller's X button
- **THEN** X presses B in game, all other bindings are unchanged, and the cell shows "X"

#### Scenario: Move an input between actions
- **WHEN** the South button is bound to A and the user binds South to Start
- **THEN** South presses Start, and A's controller cell no longer lists South

#### Scenario: Clear and reset
- **WHEN** the user selects Turbo's controller cell and presses Delete
- **THEN** no controller input activates Turbo
- **WHEN** the user then presses "Reset to Defaults"
- **THEN** the default mapping (Left Shoulder → Turbo, …) is restored

#### Scenario: Cancel capture
- **WHEN** the user double-clicks a controller cell and presses Esc
- **THEN** the binding is unchanged

#### Scenario: Keyboard and controller side by side
- **WHEN** Player 1 is selected
- **THEN** each row shows the action's keyboard key and its controller input(s) together
