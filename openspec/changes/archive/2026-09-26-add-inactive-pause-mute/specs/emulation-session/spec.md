# Spec Delta

## ADDED Requirements

### Requirement: Inactive sessions
A session SHALL be considered inactive while keyboard focus is in another application, or in another game window that is not its link-cable partner. Its own tool windows (debugger console, memory viewer, VRAM viewer, cheats, cheat search, printer), its link-cable partner's windows and application-wide dialogs (Preferences, palette editor, file dialogs) SHALL count as active. Focus changes SHALL be debounced so that switching between these windows does not register as losing focus.

#### Scenario: Switching to the debugger
- **WHEN** the user moves from a game window to that game's debugger console
- **THEN** the session remains active

#### Scenario: Switching to another game
- **WHEN** two unlinked games are open and the user focuses the second
- **THEN** the first session becomes inactive

### Requirement: Pause when inactive
Preferences → Emulation SHALL offer "Pause when inactive" (setting `GBPauseWhenInactive`, default off). When enabled, a running session that becomes inactive SHALL pause. It SHALL resume when it becomes active again, but only if the pause was caused by inactivity. A pause the user made (Pause menu, debugger break or hotkey) SHALL be kept. GBS playback SHALL NOT be affected.

#### Scenario: Default behavior
- **WHEN** the option is off and the user switches to another application
- **THEN** the game keeps running

#### Scenario: Pause and resume
- **WHEN** the option is on and the user switches to another application and back
- **THEN** the game pauses while away and resumes on return

#### Scenario: User pause is respected
- **WHEN** the option is on, the user pauses the game, switches away and comes back
- **THEN** the game stays paused
