# Spec Delta

## Purpose

Defines audio output, its adjustable processing, per-channel muting, recording, and the GBS music-player interface.

## ADDED Requirements

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
