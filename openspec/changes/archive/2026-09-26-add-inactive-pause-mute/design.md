# Design

## Context

`MainWindow` already tracks activation for background controllers (`g_lastActiveWindow`). `EmulatorSession::setMuted()` persists `Mute`, and the audio client is (re)created in `preRun()` / `startAudio()`.

## Decisions

- **Focus source:** `QGuiApplication::focusWindowChanged`, debounced by 150 ms, then re-evaluated for every `MainWindow`. The active top-level widget is classified as follows:
  - none (another app) → inactive;
  - a `MainWindow` → active iff it is this window or the link-cable partner's;
  - anything else → an owned tool window (the property `sameboyOwner` points to its `MainWindow`, and it is active iff the owner is this window or the partner) or an app-wide dialog (active).
- **Pause bookkeeping:** `MainWindow::m_pausedForInactivity` is set only when this code calls `stop()` on a running, non-debugger-stopped session. It is cleared by any user pause toggle. On reactivation it resumes only if the flag is set. For linked sessions only the master runs, and partner focus counts as active, so both stay consistent.
- **Mute:** `EmulatorSession::setInactiveMuted(bool)` stops or starts the audio client through the same path as `setMuted()` but never writes `Mute`. `preRun()` honors it, so a session resumed while inactive stays silent. Recording is untouched because it taps the core, not the audio client.
- **GBS exemption:** both options are skipped for `isGBS()` sessions.
- **Keys:** `GBPauseWhenInactive` and `GBMuteWhenInactive` are registered in `Settings::registerDefaults()` (false), with a comment noting they are Qt-only.

## Risks / Trade-offs

- [Wayland focus quirks: some compositors briefly report no focus window during popups] → The 150 ms debounce covers it. Menus and popups are owned by the window and don't change the active top-level widget.
