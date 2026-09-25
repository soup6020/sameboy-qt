# Spec Delta

## Purpose

Defines emulated serial and infrared accessories (Printer, Workboy, link cable between sessions) and the Game Boy Camera feed.

## ADDED Requirements

### Requirement: Connect menu
The Connect menu SHALL offer None, Game Link Cable & Infrared (a submenu listing other open sessions), Game Boy Printer and Workboy. Items SHALL be exclusive and SHALL show which is connected. The link-cable submenu SHALL be disabled when no other session is open.

#### Scenario: Connect printer
- **WHEN** the user selects Game Boy Printer
- **THEN** any link cable is disconnected and the printer is attached

### Requirement: Printer feed
Printed images SHALL accumulate in a Printer window as a 160-pixel-wide strip (shown at 2×) with top and bottom margins, and the window SHALL appear when printing starts. A busy indicator SHALL show during printing. The window SHALL offer Save (PNG at 1×) and Print (system print dialog). A new feed SHALL start when printing begins while the window is hidden.

#### Scenario: Save printout
- **WHEN** a game prints two images and the user saves the feed
- **THEN** a 160-pixel-wide PNG with both images and their margins is written

### Requirement: Workboy
Connecting the Workboy SHALL attach it with a clock offset persisted in settings (`GBWorkboyTimeOffset`).

#### Scenario: Workboy clock
- **WHEN** a game sets the Workboy time
- **THEN** the offset from system time is persisted and reused next time

### Requirement: Link cable between sessions
Connecting a session to another open session SHALL link their serial ports bit-by-bit and their infrared LEDs. Both SHALL run on one emulation thread with cycle-proportional interleaving. The partner SHALL run with turbo mode and an uncapped clock, so the primary session paces both. Disconnecting or closing either session SHALL restore independent running and turbo caps. With a link cable, keyboard Player 2 bindings SHALL control the partner session.

#### Scenario: Trade via link
- **WHEN** two sessions of a trading game are linked and both start a link trade
- **THEN** the trade completes

#### Scenario: Close linked partner
- **WHEN** one of two linked sessions is closed
- **THEN** the other continues running unlinked

### Requirement: Game Boy Camera
When a Game Boy Camera cartridge requests an image, the application SHALL capture a frame from the default system camera and take its luminance. The frame SHALL be scaled to cover 128×112 and center-cropped. If no camera is available or permission is denied, it SHALL supply black.

#### Scenario: No camera
- **WHEN** no camera device exists and the game requests a photo
- **THEN** the core receives an update with all-black pixels and emulation continues
