# audio Specification

## Purpose
Defines audio output, its adjustable processing, per-channel muting, recording, and the GBS music-player interface.

## Requirements

### Requirement: Audio output
Each running session SHALL output stereo audio at 96 kHz or the device rate. Buffering SHALL limit latency (drop excess beyond roughly 50 ms of backlog), and underruns SHALL be filled with silence. Audio SHALL be silent while paused or stopped in the debugger.

#### Scenario: Debugger break silences audio
- **WHEN** the debugger stops execution
- **THEN** audio output is silent until execution continues

### Requirement: Volume and mute
A Volume slider (0–100%) SHALL scale output. Emulation → Mute Sound (Ctrl+M) SHALL toggle and persist mute. Unmuting while volume is 0 SHALL show a warning.

#### Scenario: Mute persists
- **WHEN** the user mutes and reopens the application
- **THEN** new sessions start muted

### Requirement: Audio processing settings
The application SHALL expose the high-pass filter (Disabled/Keep DC offset, Accurate, Preserve waveform) and the interference volume (0–100%), and apply them live.

#### Scenario: Preserve waveform
- **WHEN** the high-pass filter is set to "Preserve waveform"
- **THEN** the core's high-pass mode is set to remove DC offset while preserving the waveform

### Requirement: Channel muting
Develop → Audio Channels SHALL toggle Square 1, Square 2, Wave and Noise (Alt+1..4) per session, shown as checkmarks.

#### Scenario: Mute noise channel
- **WHEN** the user unchecks Noise Channel
- **THEN** channel 4 no longer contributes to output

### Requirement: Audio recording
Emulation → Start Audio Recording… (Ctrl+Shift+A) SHALL prompt for a file and a format: AIFF, WAV, or raw PCM (stereo 96 kHz 16-bit LE). It SHALL record until "Stop Audio Recording" is chosen. Errors SHALL be reported with the system error text.

#### Scenario: Record WAV
- **WHEN** the user starts a WAV recording, plays 2 s, and stops
- **THEN** a valid WAV file with about 2 s of audio exists and the OSD shows "Audio recording ended"

### Requirement: GBS player
Opening a `.gbs` file SHALL show a fixed-size player window instead of the screen. The window SHALL show title, author and copyright (with fallbacks "GBS Player", "Unknown Composer" and "Missing copyright information"). It SHALL have a track selector starting at the file's first track, play/pause, previous/next with wrap-around, and a waveform visualizer tinted by the selected palette.

#### Scenario: Next track wraps
- **WHEN** the last track is selected and the user presses Next
- **THEN** track 1 starts playing

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
