# Spec Delta

## Purpose

Defines the memory viewer/editor for inspecting and modifying Game Boy memory spaces and banks, equivalent to the Cocoa HexFiend-based Memory window.

## ADDED Requirements

### Requirement: Address spaces and banks
Develop → Show Memory SHALL open a per-session hex view with an Address Space selector:
- Entire Space: $0000–$FFFF, no bank control.
- ROM: $0000–$7FFF.
- Video RAM: $8000–$9FFF.
- Cartridge RAM: $A000–$BFFF.
- RAM: $C000–$DFFF.

It SHALL have a Bank field accepting debugger expressions. The bank SHALL be reduced modulo the number of banks available in that space. Line numbers SHALL show the address offset for the space in hexadecimal.

#### Scenario: Select ROM bank 5
- **WHEN** the user selects ROM and enters `$5`
- **THEN** $4000–$7FFF shows ROM bank 5 contents

### Requirement: Live refresh
Visible memory views SHALL refresh at least 4 times per second, and after debugger output.

#### Scenario: Changing RAM
- **WHEN** the game modifies WRAM while the viewer shows RAM
- **THEN** the displayed bytes update within 250 ms

### Requirement: Editing
The user SHALL be able to overwrite bytes in hex or ASCII. Writes SHALL go to the backing memory of the selected space and bank. In Entire Space mode, cartridge RAM and I/O writes SHALL use core memory writes performed atomically. ROM edits SHALL mark the session as having modified ROM.

#### Scenario: Edit WRAM
- **WHEN** the user types `FF` over $C000
- **THEN** reading $C000 in the debugger yields $FF

### Requirement: Go to expression
A Go To field SHALL evaluate a debugger expression. It SHALL switch space and bank according to the result's address and bank, then select and scroll to the address. Errors SHALL be shown next to the field.

#### Scenario: Go to banked symbol
- **WHEN** the user enters `$3:$4100`
- **THEN** the view switches to ROM bank 3 and selects $4100

### Requirement: Address description
A status line SHALL show the selection's address and the debugger's symbolic description of it.

#### Scenario: Describe address
- **WHEN** the cursor is on $FF40
- **THEN** the status line includes the `rLCDC` symbol
