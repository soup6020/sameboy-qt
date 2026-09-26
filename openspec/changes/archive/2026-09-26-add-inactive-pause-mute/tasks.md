# Tasks

## 1. Implementation

- [x] 1.1 Register `GBPauseWhenInactive` / `GBMuteWhenInactive` (default false) and add the checkboxes to the Emulation and Audio tabs; verify with the settings-defaults test and the tab grab
- [x] 1.2 `EmulatorSession::setInactiveMuted` (no persistence, honored by `preRun`); verify in `pauseAndMuteWhenInactive` that `Mute` and the Mute Sound state are unchanged and audio resumes on focus
- [x] 1.3 Focus classification in `MainWindow` (owner property on tool windows, link partner, dialogs, 150 ms debounce) plus pause/resume bookkeeping and GBS exemption; verify with the `pauseAndMuteWhenInactive` test (two windows, focus switching, user pause kept)

## 2. Verification

- [x] 2.1 Manual check by the user on their desktop (switching apps and windows with both options on and off)
- [x] 2.2 Build warning-free, `scripts/lint.sh` clean, tests pass, `openspec validate --strict` passes
