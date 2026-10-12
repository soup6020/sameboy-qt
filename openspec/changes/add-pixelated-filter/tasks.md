# Tasks

## 1. Implementation

- [x] 1.1 `Settings`: change the `GBFilter` default to `Pixelated` and add the `GBQtSettingsVersion` migration (fresh store → current version; existing store without `GBFilter` → `NearestNeighbor`); verify with settings tests covering fresh, upgraded and explicit-value stores
- [x] 1.2 Preferences video tab: add "Pixelated" after "Nearest neighbor" and rename "Nearest neighbor (Pixelated)"; verify with the tab grab
- [x] 1.3 Software renderer: no smoothing for Pixelated; verify the GL path renders Pixelated without falling back (Xvfb check)

## 2. Verification

- [x] 2.1 Build warning-free, `scripts/lint.sh` clean on touched files, tests pass, `openspec validate --strict` passes
- [ ] 2.2 Manual check by the user: an existing install keeps its filter, and Pixelated looks right on their display
