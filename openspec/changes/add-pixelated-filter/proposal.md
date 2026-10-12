# Proposal

## Why

SameBoy 1.0.4 adds a "Pixelated" scaling filter (`Shaders/Pixelated.fsh`). It's a sharp bilinear filter: pixels stay crisp, and only the boundary between source pixels is blended, so non-integer scales no longer produce uneven pixel widths. Cocoa now uses it as the default `GBFilter` and renames "Nearest neighbor (Pixelated)" to "Nearest neighbor". The Qt frontend is pinned to 1.0.4 but doesn't offer the filter yet.

## What Changes

- Add "Pixelated" to the scaling filter list, right after "Nearest neighbor", as in `Preferences.xib`.
- Rename "Nearest neighbor (Pixelated)" to "Nearest neighbor".
- Change the registered `GBFilter` default to `Pixelated`, matching `GBApp.m`.
- **Deviation from Cocoa:** existing installs keep `NearestNeighbor`. Cocoa's registered defaults only cover keys that were never saved, so a Cocoa user who never changed the filter switches to Pixelated after upgrading. Here a one-time settings migration stores `GBFilter = NearestNeighbor` for existing stores that have no `GBFilter` key, so the picture doesn't change on upgrade. Fresh installs get Pixelated.
- Software rendering fallback: draw Pixelated without smoothing, like Nearest neighbor.

## Capabilities

### Modified Capabilities
- `display`: the Pixelated filter is added to the scaling filter list.
- `preferences`: new `GBFilter` default, and a settings migration that keeps existing installs on Nearest neighbor.

## Impact

- `src/settings/Settings.cpp` (default value and migration), `src/ui/PreferencesDialog.cpp` (filter list), `src/ui/ScreenWidget.cpp` (software path).
- New Qt-only settings key `GBQtSettingsVersion` (integer) to record which one-time migrations have run.
- No build changes: `Shaders/` is already staged in full from the submodule.
