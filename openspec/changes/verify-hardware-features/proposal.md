# Proposal

## Why

`add-qt-frontend` implemented every capability, but a few paths could only be exercised with specific ROMs or peripherals that weren't available when it was built. This change tracks their verification separately, so the initial change could be archived as the project's spec baseline.

## What Changes

- No behavior changes. The change only verifies already-specified behavior against real ROMs and hardware, and fixes any bugs found under the existing requirements.

## Capabilities

### New Capabilities
<!-- None -->

### Modified Capabilities
<!-- None: verification only (skip_specs). -->

## Impact

- Possibly bug fixes in `src/` if verification finds problems. Specs are unchanged.
