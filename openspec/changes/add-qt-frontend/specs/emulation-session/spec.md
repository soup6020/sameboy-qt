# Spec Delta

## Purpose

Defines how games are opened and run: one emulation session per window, with its lifecycle, model selection, boot ROMs, battery saves and ROM-level operations, mirroring the Cocoa document model.

## ADDED Requirements

### Requirement: Open game files
The application SHALL open `.gb`, `.gbc`, `.isx`, `.gbs` and `.gbcart` cartridge-instance directories from File → Open, from the recent-files menu, from command-line arguments and by dropping files on a window. Each opened file SHALL get its own window and independent emulator instance.

#### Scenario: Open two ROMs
- **WHEN** the user opens `a.gb` and then `b.gbc`
- **THEN** two windows run independently, each titled with its file name

#### Scenario: Load failure
- **WHEN** the ROM cannot be loaded
- **THEN** an error dialog shows the core's log output (or "Could not load ROM") and no session window remains

#### Scenario: Load warnings
- **WHEN** the core logs warnings while loading a ROM that otherwise loads
- **THEN** the warnings are shown once in a non-modal notice attached to the window

### Requirement: Idle window
When no game is open, the application SHALL show an idle window. The idle window SHALL display upstream's SDL frontend logo (`SDL/background.bmp`) recoloured with the selected monochrome palette. It SHALL keep the logo's aspect ratio and offer File (Open…, Open Recent, Quit), Edit (Preferences…) and Help menus. Launching without file arguments SHALL show the idle window, not a file dialog. Opening a game SHALL replace the idle window. Closing the last game window SHALL return to the idle window, while Quit SHALL exit the application.

#### Scenario: Launch without arguments
- **WHEN** the application starts with no files
- **THEN** the idle window shows the logo and no file dialog opens

#### Scenario: Palette follows preferences
- **WHEN** the monochrome palette is changed while the idle window is visible
- **THEN** the logo is redrawn in the new palette's colours

#### Scenario: Closing the last game
- **WHEN** the only game window is closed with File → Close
- **THEN** the idle window appears and the application keeps running

#### Scenario: Open from the idle window
- **WHEN** a ROM is dropped onto the idle window
- **THEN** a game window opens and the idle window closes

### Requirement: Session lifecycle controls
The Emulation menu SHALL provide Reset (Ctrl+R), Quick Reset (Ctrl+Alt+R), Reload ROM (Ctrl+Shift+R), and Pause (Ctrl+P) as a checkable toggle. Resetting SHALL reload the ROM, battery, cheats and symbol files. After reset the OSD SHALL show "SameBoy v<version>", the ROM title and its CRC32.

#### Scenario: Pause and resume
- **WHEN** the user toggles Pause on a running session
- **THEN** emulation and audio stop and the Pause item is checked; toggling again resumes

#### Scenario: Quick reset
- **WHEN** the user chooses Quick Reset
- **THEN** the core performs a quick reset without changing the emulated model

### Requirement: Emulated model selection
The Emulated Model submenu SHALL offer Pick Automatically, Game Boy, Game Boy Pocket/Light, Super Game Boy, Game Boy Color and Game Boy Advance. Selecting one SHALL persist it and reset the session. In automatic mode the model SHALL be CGB for CGB-flagged ROMs, SGB for SGB-flagged ROMs, CGB for Nintendo-licensed ROMs (for boot ROM palettes), and DMG otherwise. The specific hardware revision used for each family SHALL come from Preferences.

#### Scenario: Automatic model for an SGB game
- **WHEN** a ROM with header byte $146 = $03 and no CGB flag is opened in automatic mode
- **THEN** the session runs as the configured Super Game Boy revision

#### Scenario: Revision preference changes
- **WHEN** the user changes the Game Boy Color revision in Preferences while a CGB session is running
- **THEN** that session resets using the new revision

### Requirement: Boot ROM selection
The application SHALL load the bundled boot ROM matching the requested type by default. When the user selects a custom boot ROM folder, it SHALL prefer `<name>.bin` from that folder. It SHALL fall back from `cgbE_boot` to `cgb_boot` and from `agb0_boot` to `agb_boot` when those are absent.

#### Scenario: Custom boot ROM present
- **WHEN** the custom folder contains `dmg_boot.bin` and a DMG session starts
- **THEN** the custom boot ROM is used

### Requirement: Battery saves
Cartridge RAM SHALL be loaded from `<rom>.sav` (or `battery.sav` inside a `.gbcart`) on load. It SHALL be written when the battery becomes clean after being dirty (checked every 0.25 s), and again when the session stops or closes. ISX files SHALL also try `<rom>.ram`. An unwritable save path SHALL produce a warning.

#### Scenario: Save persists across sessions
- **WHEN** a game writes to battery RAM, then the window is closed and the ROM reopened
- **THEN** the saved data is present

### Requirement: Cartridge instances
File → New Cartridge Instance SHALL create a `.gbcart` directory containing a `rom.gbl` reference to the current ROM (relative path, then absolute). Opening a `.gbcart` SHALL keep saves, states and cheats inside that directory.

#### Scenario: Separate saves per instance
- **WHEN** two instances of the same ROM are created and opened
- **THEN** each keeps its own `battery.sav`

### Requirement: Hot swap, reload and ROM modification saving
File → Hot Swap Cartridge (Ctrl+Alt+O) SHALL load a different ROM into the running session. It SHALL save the current battery first and refuse ROMs already open elsewhere. Reload ROM SHALL discard in-memory ROM edits. When ROM bytes are edited through the memory viewer, the window SHALL be marked modified. "Save ROM Modifications" and "Save ROM Modifications As…" SHALL write the modified ROM.

#### Scenario: Save modified ROM
- **WHEN** the user edits a ROM byte in the memory viewer and chooses Save ROM Modifications
- **THEN** the ROM file on disk contains the edited byte and the window is no longer marked modified

### Requirement: Session settings applied live
RTC mode, rewind length, turbo cap, rumble mode, color correction, light temperature, high-pass filter, interference volume and border mode SHALL apply to running sessions without restart.

#### Scenario: Change rewind length
- **WHEN** the rewind length preference changes while running
- **THEN** the core's rewind buffer length is updated atomically between emulation steps

### Requirement: File change detection
When a session's window is activated and the ROM file's modification time has changed since load, the session SHALL reset with the new ROM.

#### Scenario: ROM rebuilt externally
- **WHEN** a developer rebuilds the ROM file and focuses the SameBoy window
- **THEN** the session resets using the new ROM

### Requirement: Alarm notifications
When a session stops with a pending cartridge alarm (`GB_time_to_alarm` > 0), the application SHALL schedule a desktop notification ("<Game> Played an Alarm") at that time while it keeps running. Starting the same game again SHALL cancel it.

#### Scenario: Alarm clock game closed
- **WHEN** a game with a pending alarm in 60 seconds is closed and the app stays open
- **THEN** a desktop notification appears about 60 seconds later
