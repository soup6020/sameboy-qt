# save-states Specification

## Purpose
Defines save-state slots and how states are saved and loaded, including drag-and-drop and legacy-file compatibility, mirroring Cocoa.

## Requirements

### Requirement: Ten slots with shortcuts
Emulation → Save State SHALL offer Slots 1–10 (Ctrl+1..9, Ctrl+0). Load State SHALL offer the same slots (Ctrl+Shift+1..0). States SHALL be stored as `<rom>.s<N>`, or `state.s<N>` inside a `.gbcart`. They SHALL be taken atomically between emulation steps.

#### Scenario: Save and load
- **WHEN** the user saves slot 2 and later loads slot 2
- **THEN** emulation resumes from the saved point and the OSD shows "State saved" then "State loaded"

### Requirement: State errors surfaced
A failed save SHALL show "Failed to write save state." A failed load SHALL beep and show the core's log output.

#### Scenario: Load missing slot
- **WHEN** the user loads an empty slot with no legacy file
- **THEN** an error is shown and emulation state is unchanged

### Requirement: Legacy slot fallback
When `<rom>.s<N>` does not exist (non-`.gbcart`), loading SHALL try `<rom>.sn<N>`.

#### Scenario: Legacy file
- **WHEN** only `game.sn1` exists and the user loads slot 1
- **THEN** `game.sn1` is loaded

### Requirement: Drag and drop states
Dropping a save-state file (as recognized by the core) onto a session's screen SHALL load it into that session.

#### Scenario: Drop state
- **WHEN** a valid state file is dropped on the screen
- **THEN** it is loaded and the OSD shows "State loaded"
