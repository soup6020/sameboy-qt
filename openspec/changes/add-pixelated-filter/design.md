# Design

## Context

`Settings::value()` returns the stored value if there is one, and otherwise the default registered in memory. Defaults are never written to disk. The Preferences combo box only writes when the user picks an entry. So any install where the filter was never changed has no `GBFilter` key, and changing the registered default would change the filter for those users.

## Decisions

### Versioned one-time migration
Add a Qt-only key, `GBQtSettingsVersion`, holding the number of the last migration applied. Migrations run in `Settings::Settings()` after the legacy-location migration and before `registerDefaults()`:

- **Fresh store** (empty after the legacy migration): there is no previous behavior to preserve, so set the version to the current value without running any migration.
- **Existing store** with a version below 1 and no `GBFilter` key: store `GBFilter = NearestNeighbor`, then set the version to 1.

**Why not a dedicated boolean flag:** later Cocoa default changes, which upstream makes occasionally, can add migration steps with the same mechanism.

**Why not key off `GBThemesVersion`:** that key tracks the core version. A store created fresh on 1.0.4 that later upgrades to 1.0.5 would wrongly look like an "existing install that predates Pixelated".

### Software rendering
The QPainter fallback can't run shaders. Pixelated's goal is crisp pixels, so it maps to unsmoothed scaling, the same as Nearest neighbor. Every other filter keeps using smooth scaling.

## Risks

- A user who deliberately reset their settings by deleting single keys, without deleting the whole file, would also be pinned to Nearest neighbor. This is acceptable: they can still pick Pixelated in Preferences.
