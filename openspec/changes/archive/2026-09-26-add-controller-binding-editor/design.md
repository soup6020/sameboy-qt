# Design

## Context

Controller mappings are stored per controller like JoyKit's: `JoyKitInstanceMapping[uniqueId]` and `JoyKitNameMapping[deviceName]`, each `{inputId: GamepadAction}`. Input ids are `b<SDL button>` and `a<SDL axis>`. Special keys `AnalogTurbo`/`AnalogUnderclock` name an axis id. A controller without either entry uses the built-in defaults in `GamepadManager`. Players are *assigned* controllers; mappings do not belong to players.

## Goals / Non-Goals

**Goals:** show and edit individual controller bindings in the existing Controls table; keep the storage format so existing wizard mappings keep working.

**Non-Goals:** per-player controller mappings; mapping multiple inputs to one action from the editor (defaults may still have several, and the editor preserves them until the action is rebound); remapping analog sticks.

## Decisions

- **One table, three columns (Action | Keyboard | Controller).** The table always lists all 15 actions. Keyboard cells that can't be bound for the selected player (Turbo/Rewind/Slow-Motion for players 2–4; the hotkeys for everyone) show "—" and aren't editable. *Alternative:* a separate controller table. Rejected: the user asked for side-by-side bindings.
- **Controller column follows a controller selector, not the player selector.** This matches how mappings are stored. It defaults to the selected player's preferred controller (`JoyKitDefaultControllers`), else the first connected one. With no controller connected, the column shows "—" and the selector reads "No controllers connected".
- **Cell selection instead of row selection.** Delete clears exactly the focused cell's binding. Esc cancels a pending capture of either kind (so Esc can't be bound as a game key from this table, which is an acceptable trade-off).
- **Capture uses `GamepadManager::rawInput`,** as the wizard does. Only presses from the selected controller count, and the 250 ms debounce is shared with the wizard.
- **Mapping helpers move into `GamepadManager`:**
  - `mappingForEditing(controller)`: the custom mapping, or the defaults if there is none;
  - `setMapping(controller, map)`: writes both the instance and name entries;
  - `resetMapping(controller)`: removes both entries;
  - `inputDisplayName(controller, inputId)`: uses SDL's button labels for face buttons (`SDL_GetGamepadButtonLabel`) and fixed names otherwise.

  `PreferencesDialog` stays UI-only.
- **Settings change signals refresh the table,** so wizard progress and edits made elsewhere show up without extra plumbing.

## Risks / Trade-offs

- [Name mapping is shared by every controller of the same model] → Same as the wizard today. Editing one DualSense changes the name-level default for other DualSenses that have no instance mapping. This is documented in the spec as "stored the same way as the wizard's".
