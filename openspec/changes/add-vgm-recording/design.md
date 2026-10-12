# Design

## Context

`EmulatorSession::startAudioRecording` passes the format straight to `GB_start_audio_recording`. The core handles VGM entirely on its own: it writes the header and APU snapshot at start, logs register writes during `GB_run`, and patches the header when recording stops. Unlike the PCM formats, VGM doesn't need an output sample rate.

## Decisions

- Use the dialog's file-type filter as the format selector, as the existing formats already do. This keeps the Qt port's adaptation of Cocoa's accessory popup. Cocoa's "VGM Log" label is used with the `*.vgm` pattern.
- Keep WAV as the preselected format. That existing Qt choice is unrelated to this change.
