# Spec Delta

## Purpose

Defines cheat management and RAM cheat search, equivalent to the Cocoa Cheats and Cheat Search windows.

## ADDED Requirements

### Requirement: Enable cheats
Cheats → Enable Cheats (Ctrl+Shift+C) SHALL toggle cheat application for the session and show its state.

#### Scenario: Disable cheats
- **WHEN** cheats are disabled
- **THEN** no cheat modifies memory reads

### Requirement: Cheats window
Cheats → Show Cheats SHALL list cheats with columns Delete, Enabled, Description and Action. Action SHALL read `[$addr] = $vv`, `[$bank:$addr] = $vv`, or with an old-value condition `[$addr]($oo) = $vv`. A final "Add Cheat…" row SHALL create new cheats. Selecting a row SHALL populate editors for address (`$addr` or `$bank:$addr`), value, optional old value and description. Edits SHALL apply immediately.

#### Scenario: Add cheat manually
- **WHEN** the user selects "Add Cheat…", enters address `$C0A0` and value `$63`
- **THEN** a new cheat appears and, when enabled, reads of $C0A0 return $63

### Requirement: Code import
The window SHALL import GameShark and Game Genie codes with a description. An invalid code SHALL be rejected with "This code is not a valid GameShark or Game Genie code".

#### Scenario: Import GameShark
- **WHEN** the user imports `01FF16D0`
- **THEN** an enabled cheat writing $FF to $D016 is added

### Requirement: Cheat persistence
Cheats SHALL be loaded from `<rom>.cht` (or `cheats.cht` in a `.gbcart`) on load. They SHALL be saved when the session stops or closes.

#### Scenario: Reopen game
- **WHEN** a cheat is added and the game is closed and reopened
- **THEN** the cheat is present

### Requirement: Cheat search
Cheats → Search Cheats SHALL open a search window with:
- data type: 8-bit, 16-bit or 16-bit big-endian
- search condition: Any, Is Equal To…, Is Different From…, Is Greater Than…, Is Equal or Greater Than…, Is Less Than…, Is Equal or Less Than…, Did Change, Did Not Change, Did Increase, Did Decrease or Custom…
- operand and expression fields
- Search and Reset buttons
- a results table (Address, Previous Value, Current Value)

The data type SHALL be locked after the first search until reset. The current value SHALL be editable via a debugger expression. "Add Cheat" SHALL create one cheat (8-bit) or two (16-bit) from the selected result, enable cheats and show the Cheats window.

#### Scenario: Narrow a value
- **WHEN** the user searches "Is Equal To… 3", then the value changes to 2 in-game, then searches "Did Decrease"
- **THEN** results contain only addresses that were 3 and are now lower

#### Scenario: Invalid expression
- **WHEN** a custom expression fails to parse
- **THEN** the error is shown and results are unchanged
