# Spec Delta

## Purpose

Defines the developer-mode debugger console and its controls, exposing SameBoy's built-in debugger like the Cocoa Debug Console.

## ADDED Requirements

### Requirement: Developer mode
Develop → Developer Mode SHALL be a persisted toggle. When it is on:
- Break Debugger (Ctrl+C in the screen) SHALL be enabled.
- Debugger output SHALL bring the console window to the front.
When it is off, Break Debugger SHALL be disabled.

#### Scenario: Break
- **WHEN** developer mode is on and the user chooses Break Debugger
- **THEN** execution stops, the console shows "^C" and the debugger prompt, and the console input is focused

### Requirement: Console window
Develop → Show Console SHALL open a per-session console titled with the file name. It SHALL contain:
- a monospace output view with bold and underlined log attributes
- an input line with history (empty input repeats the last command) and tab completion
- Continue/Interrupt, Step Out, Step Over, Step Backward and Step Into buttons that issue `continue`/`interrupt`, `finish`, `next`, `backstep` and `step`
- a Help button
- a CPU-load graph and percentage

Develop → Clear Console (Ctrl+K) SHALL clear the output.

#### Scenario: Step button
- **WHEN** execution is stopped and the user clicks Step Into
- **THEN** the `step` command is echoed after ">" and executed

#### Scenario: Command while running
- **WHEN** the user enters `registers` while the game runs
- **THEN** the session breaks, executes the command and waits at the prompt

### Requirement: Side view
The console SHALL have an editable list of commands (default `registers` and `backtrace`) in a collapsible side panel. The commands SHALL run every time the debugger stops. Their output SHALL appear, each under a bold "<command>:" heading, in a side output view that clears when execution continues.

#### Scenario: Side view refresh
- **WHEN** the debugger stops at a breakpoint
- **THEN** the side output shows fresh `registers` and `backtrace` output

### Requirement: Debugger controls reflect state
Stepping buttons SHALL be enabled only while stopped. The Continue button SHALL become an Interrupt button while running.

#### Scenario: Running state
- **WHEN** execution is running
- **THEN** step buttons are disabled and the primary button reads "Interrupt"

### Requirement: Symbols and help
The debugger SHALL load upstream `registers.sym` and `<rom>.sym` on load and reset. Help → Debugger Help SHALL open https://sameboy.github.io/debugger/.

#### Scenario: ROM symbols
- **WHEN** `game.sym` exists next to `game.gb`
- **THEN** its labels are available in debugger expressions

### Requirement: Debugger font
The console, memory viewer and VRAM object/palette views SHALL use the monospace font and size from Preferences, and SHALL update when the setting changes.

#### Scenario: Font size change
- **WHEN** the debugger font size is changed to 14
- **THEN** console text re-renders at 14 pt
