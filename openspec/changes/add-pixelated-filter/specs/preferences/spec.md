# Spec Delta

## MODIFIED Requirements

### Requirement: Settings persistence with Cocoa key names
Settings SHALL persist in the platform-native settings store, at `~/.config/sameboy-qt/sameboy-qt.conf` on Linux. Settings found in the previous location (`~/.config/SameBoy/SameBoy-Qt.conf`) SHALL be migrated once when the new store is empty. They SHALL use the Cocoa defaults key names (for example `GBFilter`, `GBColorCorrection`, `GBEmulatedModel`) and the same default values.

When an upstream default changes, an existing settings store SHALL keep its previous effective value. One-time migrations SHALL be tracked by the Qt-only key `GBQtSettingsVersion`, and a fresh store SHALL start at the current version without running any migration. Migration 1 SHALL store `GBFilter = NearestNeighbor` in an existing store that has no `GBFilter` key.

#### Scenario: Settings location on Linux
- **WHEN** a setting is changed on Linux
- **THEN** it is written to `$XDG_CONFIG_HOME/sameboy-qt/sameboy-qt.conf` (default `~/.config/sameboy-qt/`)

#### Scenario: Default color correction
- **WHEN** the application runs with no stored settings
- **THEN** color correction is "Modern – Balanced" and the CGB revision is "CPU CGB E"

#### Scenario: Default filter on a fresh install
- **WHEN** the application runs with no stored settings
- **THEN** the scaling filter is "Pixelated"

#### Scenario: Filter kept on upgrade
- **WHEN** an existing settings store that has no `GBFilter` key is opened for the first time by a version with migration 1
- **THEN** `GBFilter` is stored as `NearestNeighbor` and the scaling filter is "Nearest neighbor"

#### Scenario: Explicit filter kept on upgrade
- **WHEN** an existing settings store has `GBFilter = CRT`
- **THEN** the migration leaves it unchanged
