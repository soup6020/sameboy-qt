# preferences Specification

## Purpose
Defines the Preferences window, persistence of settings and the monochrome palette theme editor, matching the Cocoa Preferences tabs.

## Requirements

### Requirement: Preferences window tabs
Preferences (Ctrl+,) SHALL have four tabs:
- **Emulation**: Game Boy / GBC / GBA / SGB revisions, boot ROM location (built-in or custom folder), rewind duration (Disabled, 10 s, 30 s, 1 min, 2 min, 5 min, 10 min), RTC mode (Sync to system clock, Accurate) and turbo cap (checkbox plus 150–400% slider).
- **Video**: filter, apply filters to screenshots, color correction, ambient light temperature, frame blending, monochrome palette, display border, keep aspect ratio, force integer scale, on-screen display and monospace (debugger) font and size.
- **Audio**: volume, high-pass filter and interference volume.
- **Controls**: player selector; a controller selector; a mapping table with Action, Keyboard and Controller columns; "Reset to Defaults" for the selected controller; controller configuration wizard; preferred controller per player; motion options; rumble mode and strength; hotkey actions; analog speed controls; faux analog; and background controllers.

#### Scenario: Change and reopen
- **WHEN** the user changes a setting, closes Preferences and restarts the app
- **THEN** the setting is retained

### Requirement: Settings menu
Instead of an Edit menu, every window (game windows and the idle window) SHALL have a Settings menu, in the style of ares. It SHALL contain one entry per Preferences tab (Emulation…, Video…, Audio…, Controls…), each opening Preferences on that tab, followed by Preferences… (Ctrl+,) which opens it on the last-used tab. On macOS, Preferences… SHALL move to the application menu while the tab entries stay in Settings.

#### Scenario: Jump to a tab
- **WHEN** the user chooses Settings → Audio… while a game is running
- **THEN** the Preferences window opens (or comes to the front) showing the Audio tab, and emulation keeps running

#### Scenario: Idle window
- **WHEN** no game is open and the user chooses Settings → Controls…
- **THEN** the Preferences window opens on the Controls tab

### Requirement: Settings persistence with Cocoa key names
Settings SHALL persist in the platform-native settings store, at `~/.config/sameboy-qt/sameboy-qt.conf` on Linux. Settings found in the previous location (`~/.config/SameBoy/SameBoy-Qt.conf`) SHALL be migrated once when the new store is empty. They SHALL use the Cocoa defaults key names (for example `GBFilter`, `GBColorCorrection`, `GBEmulatedModel`) and the same default values.

#### Scenario: Settings location on Linux
- **WHEN** a setting is changed on Linux
- **THEN** it is written to `$XDG_CONFIG_HOME/sameboy-qt/sameboy-qt.conf` (default `~/.config/sameboy-qt/`)

#### Scenario: Default color correction
- **WHEN** the application runs with no stored settings
- **THEN** color correction is "Modern – Balanced" and the CGB revision is "CPU CGB E"

### Requirement: Live application
Changing a preference SHALL immediately affect all open sessions where the Cocoa frontend does so. No Apply button is required.

#### Scenario: Volume change
- **WHEN** the volume slider is moved while a game plays
- **THEN** loudness changes immediately

### Requirement: Palette theme editor
Choosing "Custom…" in the palette menu SHALL open an editor with:
- a list of named themes (add, remove, rename, restore defaults, import and export `.sbp`)
- five color wells (darkest, three shades, lightest/off)
- "Distinct disabled LCD color" and "Manual mode" checkboxes
- brightness bias, hue bias and hue bias strength sliders that generate intermediate shades when not in manual mode

The default themes SHALL include the upstream Cocoa set (Canyon, Desert, Evening, Fog, Green Slate, Green Tea, Lavender, Magic Eggplant, Mystic Blue, Pink Pop, Radioactive Pea, Rose, Seaweed, Twilight).

#### Scenario: Export and import
- **WHEN** the user exports a theme and imports the resulting `.sbp`
- **THEN** a new theme with identical parameters is created, named after the file

#### Scenario: Import invalid file
- **WHEN** the user imports a file without the `SBPL` magic
- **THEN** the import is rejected with a beep and no theme is added

### Requirement: Preferences usable at any window size
Each Preferences tab SHALL keep its controls at their natural size and become scrollable when the window is smaller than the tab's content, for example when a tiling window manager ignores the minimum size. Widgets SHALL NOT overlap or be squashed. When opened as a floating window, Preferences SHALL initially be sized to fit the largest tab.

#### Scenario: Tiled into a small area
- **WHEN** the Preferences window is forced to 400×300 pixels with the Controls tab shown
- **THEN** the Controls tab scrolls, and the mapping table keeps its full size and layout
