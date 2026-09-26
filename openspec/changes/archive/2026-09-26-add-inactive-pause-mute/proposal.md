# Proposal

## Why

Players often switch away from the emulator mid-game. Some want the game to keep running (e.g. idling or grinding), others want it to stop or go quiet. Cocoa has no option for this, and the SDL frontend only partially covers it. Two opt-in settings cover both preferences without changing default behavior.

## What Changes

- New **Pause when inactive** option (Emulation tab, off by default). A game pauses when its window loses focus and resumes when focus returns, unless the user had paused it.
- New **Mute when inactive** option (Audio tab, off by default). A game's audio goes silent when its window loses focus and returns when it regains focus, without touching the persistent Mute setting.
- "Inactive" means focus is in another application or another game window. The game's own tool windows, its link-cable partner and app dialogs count as active.
- GBS files (music playback) are exempt.

## Capabilities

### New Capabilities
<!-- None -->

### Modified Capabilities
- `emulation-session`: adds the pause-when-inactive behavior and the shared definition of an inactive session.
- `audio`: adds the mute-when-inactive behavior.

## Impact

- `src/ui/MainWindow.*`: focus tracking and applying the options.
- `src/core/EmulatorSession.*`: a background-mute flag separate from the persistent mute.
- `src/ui/PreferencesDialog.cpp`: two checkboxes.
- New settings keys `GBPauseWhenInactive` and `GBMuteWhenInactive` (Qt-only; Cocoa has no equivalent). Both default to false.
