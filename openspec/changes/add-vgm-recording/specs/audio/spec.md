# Spec Delta

## MODIFIED Requirements

### Requirement: Audio recording
Emulation → Start Audio Recording… (Ctrl+Shift+A) SHALL prompt for a file and a format: AIFF, WAV, VGM log, or raw PCM (stereo 96 kHz 16-bit LE). It SHALL record until "Stop Audio Recording" is chosen. Errors SHALL be reported with the system error text.

#### Scenario: Record WAV
- **WHEN** the user starts a WAV recording, plays 2 s, and stops
- **THEN** a valid WAV file with about 2 s of audio exists and the OSD shows "Audio recording ended"

#### Scenario: Record VGM log
- **WHEN** the user chooses "VGM Log", enters a name without an extension, plays 2 s, and stops
- **THEN** a `.vgm` file starting with the `Vgm ` signature exists and the OSD shows "Audio recording ended"
