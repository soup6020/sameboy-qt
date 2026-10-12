# Tasks

## 1. Implementation

- [x] 1.1 Add "VGM Log (*.vgm)" to the Start Audio Recording dialog (Cocoa order) and map it to `GB_AUDIO_FORMAT_VGM` with the `.vgm` default extension
- [x] 1.2 Test: record VGM through `EmulatorSession` and check the `Vgm ` header and that the file grows beyond the header

## 2. Verification

- [x] 2.1 Build warning-free, `scripts/lint.sh` clean on touched files, tests pass, `openspec validate --strict` passes
- [ ] 2.2 Manual check by the user: a recorded `.vgm` plays back in a VGM player
