# Spec Delta

## MODIFIED Requirements

### Requirement: Gamepad support
Connected game controllers SHALL drive the emulator with default mappings:
- D-pad → directions; A/B → A/B; Start or X → Start; Back or Y → Select
- Left shoulder → Turbo; left/right triggers → Rewind; right shoulder → Slow-Motion

A "Configure a controller" wizard SHALL capture a button for each action in sequence, with Skip. It SHALL store per-device-instance and per-device-name mappings, and SHALL record analog axes for Turbo and Slow-Motion when an axis is used. While the wizard runs, the Controls table SHALL highlight the action being captured and show each binding as soon as it is captured.

#### Scenario: Configure controller
- **WHEN** the user runs the wizard and presses buttons for each prompt
- **THEN** the controller uses the new mapping after the wizard ends, and the Controls table shows the new bindings

## ADDED Requirements

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
