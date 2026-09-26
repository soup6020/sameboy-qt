# Spec Delta

## ADDED Requirements

### Requirement: Mute when inactive
Preferences → Audio SHALL offer "Mute when inactive" (setting `GBMuteWhenInactive`, default off). When enabled, an inactive session (as defined by the emulation-session "Inactive sessions" requirement) SHALL output no audio. Audio SHALL return when the session becomes active, unless Emulation → Mute Sound is on. This SHALL NOT change the persistent Mute setting or the Mute Sound checkmark. Audio recording SHALL continue unaffected. GBS playback SHALL NOT be affected.

#### Scenario: Default behavior
- **WHEN** the option is off and the user switches to another application
- **THEN** audio keeps playing

#### Scenario: Muted while away
- **WHEN** the option is on and the user switches to another application
- **THEN** the game goes silent, keeps running, and is audible again after switching back

#### Scenario: Persistent mute wins
- **WHEN** Mute Sound is on and the user returns to the window
- **THEN** audio stays muted
