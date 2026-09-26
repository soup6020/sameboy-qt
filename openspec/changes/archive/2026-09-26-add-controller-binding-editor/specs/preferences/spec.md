# Spec Delta

## MODIFIED Requirements

### Requirement: Preferences window tabs
Preferences (Ctrl+,) SHALL have four tabs:
- **Emulation**: Game Boy / GBC / GBA / SGB revisions, boot ROM location (built-in or custom folder), rewind duration (Disabled, 10 s, 30 s, 1 min, 2 min, 5 min, 10 min), RTC mode (Sync to system clock, Accurate) and turbo cap (checkbox plus 150–400% slider).
- **Video**: filter, apply filters to screenshots, color correction, ambient light temperature, frame blending, monochrome palette, display border, keep aspect ratio, force integer scale, on-screen display and monospace (debugger) font and size.
- **Audio**: volume, high-pass filter and interference volume.
- **Controls**: player selector; a controller selector; a mapping table with Action, Keyboard and Controller columns; "Reset to Defaults" for the selected controller; controller configuration wizard; preferred controller per player; motion options; rumble mode and strength; hotkey actions; analog speed controls; faux analog; and background controllers.

#### Scenario: Change and reopen
- **WHEN** the user changes a setting, closes Preferences and restarts the app
- **THEN** the setting is retained

## ADDED Requirements

### Requirement: Preferences usable at any window size
Each Preferences tab SHALL keep its controls at their natural size and become scrollable when the window is smaller than the tab's content, for example when a tiling window manager ignores the minimum size. Widgets SHALL NOT overlap or be squashed. When opened as a floating window, Preferences SHALL initially be sized to fit the largest tab.

#### Scenario: Tiled into a small area
- **WHEN** the Preferences window is forced to 400×300 pixels with the Controls tab shown
- **THEN** the Controls tab scrolls, and the mapping table keeps its full size and layout
