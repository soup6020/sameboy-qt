# Proposal

## Why

Controllers can only be configured with the all-at-once "Configure a controller" wizard, and the resulting bindings are never shown. Users can't see what a button does, fix one wrong binding without redoing everything, or compare controller and keyboard bindings side by side. Cocoa has the same limitation. This change deliberately goes beyond Cocoa parity because it's a clear usability gap.

## What Changes

- The Controls tab's mapping table shows a **Controller** column next to the **Keyboard** column. It lists the binding of every action for a chosen controller.
- A controller selector above the table picks whose mapping is shown and edited. It defaults to the selected player's preferred controller, else the first connected one.
- Individual bindings can be rebound: double-click a cell, then press a key or controller input. Esc cancels, and Delete or the context menu clears a binding. "Reset to Defaults" restores the chosen controller's default mapping.
- The "Configure a controller" wizard stays. The table highlights the action being captured and updates as the wizard advances.
- Storage is unchanged (`JoyKitInstanceMapping` / `JoyKitNameMapping`), so wizard-made and editor-made mappings are interchangeable.

## Capabilities

### New Capabilities
<!-- None -->

### Modified Capabilities
- `input`: Gamepad support gains per-binding editing and visible bindings, and the wizard is described in terms of the shared table.
- `preferences`: The Controls tab gains the Controller column, controller selector and reset button.

## Impact

- `src/ui/PreferencesDialog.*`: Controls tab table and capture logic.
- `src/input/GamepadManager.*`: display names for inputs, plus helpers to read, write and reset a controller's full mapping.
- No change to settings keys or to how gameplay input is routed.
