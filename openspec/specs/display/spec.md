# display Specification

## Purpose
Defines how emulated frames are presented: GPU filters, color and palette processing, borders, scaling, the on-screen display, window sizing and screenshots, equivalent to the Cocoa view.

## Requirements

### Requirement: Scaling filters
The screen SHALL be rendered with upstream's GLSL master shader and the selected filter. The choices SHALL be Nearest Neighbor, Bilinear, Smooth Bilinear, Monochrome LCD, LCD, CRT, Flat CRT, Scale2x, Scale4x, Anti-aliased Scale2x, Anti-aliased Scale4x, HQ2x, OmniScale, OmniScale Legacy and Anti-aliased OmniScale Legacy. Changing the filter SHALL take effect immediately.

#### Scenario: Switch to CRT
- **WHEN** the user selects "CRT display" in Preferences
- **THEN** all open sessions render with the CRT shader on their next frame

### Requirement: Frame blending
Frame blending SHALL support Disabled, Simple and Accurate. Accurate SHALL use alternating even/odd blending on DMG/CGB and SHALL behave as Simple on SGB.

#### Scenario: Accurate blending on SGB
- **WHEN** frame blending is Accurate and the model is SGB
- **THEN** frames are blended with the Simple ratio

### Requirement: Color correction and temperature
The application SHALL expose the core's color-correction modes: Disabled, Correct Color Curves, Modern Balanced, Modern Accurate, Modern Boost Contrast, Reduce Contrast and Harsh Reality. It SHALL also expose an ambient light temperature slider from -1 to 1.

#### Scenario: Harsh reality
- **WHEN** color correction is set to "Harsh reality"
- **THEN** CGB colors are converted with the core's low-contrast mode

### Requirement: Monochrome palettes
Monochrome models SHALL use the selected palette: Greyscale, Lime (Game Boy), Olive (Pocket), Teal (Light), or a named user theme from the palette editor.

#### Scenario: Select a theme
- **WHEN** the user selects the "Canyon" theme
- **THEN** DMG sessions immediately render with Canyon's five colors

### Requirement: Borders
Display border SHALL be Never, Super Game Boy only, or Always. Changing it SHALL resize the screen between 160×144 and 256×224 at the next frame and update the minimum window size.

#### Scenario: Always show border
- **WHEN** border is set to Always on a DMG game
- **THEN** the screen becomes 256×224 with the DMG border around the game

### Requirement: Aspect and integer scaling
The screen SHALL keep the emulated aspect ratio (letterboxed) unless "Keep aspect ratio" is off. "Force integer scale" SHALL restrict the displayed size to integer multiples of the native resolution in device pixels.

#### Scenario: Integer scale in a large window
- **WHEN** integer scaling is on and the window is 500×500 logical pixels at 1× DPR
- **THEN** a 160×144 game is drawn at 480×432, centered

### Requirement: On-screen display
When OSD is enabled, transient messages SHALL be drawn over the screen as white text with a black outline, sized relative to the emulated resolution. They SHALL use GBOSDView's timing: an animation value starting at 2.5 (6.5 for multi-line messages) that decreases by 0.1 every 25 ms, fading out once it drops below 1. Messages SHALL include state save/load, screenshots, audio recording, "Fast forwarding…", "Slow motion…" and "Rewinding…".

#### Scenario: OSD disabled
- **WHEN** OSD is disabled and a state is saved
- **THEN** no overlay text appears

### Requirement: Window sizing and fullscreen
Window → Increase Window Size (Ctrl++) and Decrease Window Size (Ctrl+-) SHALL step the window through integer multiples of the native size, within the available screen. Zoom SHALL snap to the nearest integer multiple. The last window size SHALL be remembered for new windows. Fullscreen SHALL be toggleable and SHALL hide the mouse cursor over the screen while running and no tool windows are shown.

#### Scenario: Increase size
- **WHEN** the window shows the game at 2× and the user presses Ctrl++
- **THEN** the window resizes so the game is shown at 3×

### Requirement: Layer toggles
Develop → Show Background and Window and Develop → Show Objects SHALL toggle core rendering of those layers and reflect their state as checkmarks.

#### Scenario: Hide objects
- **WHEN** the user unchecks Show Objects
- **THEN** sprites are no longer drawn

### Requirement: Screenshots
Emulation → Save Screenshot (Ctrl+S) SHALL write a PNG named "<game> – <date time>.png" into the screenshot folder. It SHALL ask for the folder the first time and add a numeric suffix if the name exists. Save Screenshot As… (Ctrl+Alt+S) SHALL prompt for a path and remember its folder. Copy Screenshot (Ctrl+Shift+S) SHALL place the image on the clipboard. When "Apply filters to screenshots" is on, screenshots SHALL use the filtered output at display resolution; otherwise they SHALL use the raw frame.

#### Scenario: Raw screenshot
- **WHEN** filters-in-screenshots is off and the user copies a screenshot of a DMG game without border
- **THEN** the clipboard holds a 160×144 image
