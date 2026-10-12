# Proposal

## Why

SameBoy 1.0.4 adds VGM logging to the core's audio recorder (`GB_AUDIO_FORMAT_VGM`). A VGM log records APU register writes rather than rendered samples, so chiptune players can replay it and it can be edited. Cocoa offers it as "VGM Log" in the Start Audio Recording format menu. The Qt frontend is pinned to 1.0.4 but only offers AIFF, WAV and raw PCM.

## What Changes

- Add "VGM Log (*.vgm)" to the Start Audio Recording dialog, in Cocoa's order: AIFF, WAV, VGM Log, Raw PCM.
- When the user picks it, record with `GB_AUDIO_FORMAT_VGM` and append `.vgm` to paths that have no extension.

## Capabilities

### Modified Capabilities
- `audio`: VGM is added to the audio recording formats.

## Impact

- `src/ui/MainWindow.cpp` (`toggleAudioRecording`). `EmulatorSession` passes the format through unchanged.
