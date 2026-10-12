# Spec Delta

## MODIFIED Requirements

### Requirement: Scaling filters
The screen SHALL be rendered with upstream's GLSL master shader and the selected filter. The choices SHALL be Nearest Neighbor, Pixelated, Bilinear, Smooth Bilinear, Monochrome LCD, LCD, CRT, Flat CRT, Scale2x, Scale4x, Anti-aliased Scale2x, Anti-aliased Scale4x, HQ2x, OmniScale, OmniScale Legacy and Anti-aliased OmniScale Legacy. Changing the filter SHALL take effect immediately. When OpenGL is unavailable, the software renderer SHALL scale without smoothing for Nearest Neighbor and Pixelated, and with smoothing for every other filter.

#### Scenario: Switch to CRT
- **WHEN** the user selects "CRT display" in Preferences
- **THEN** all open sessions render with the CRT shader on their next frame

#### Scenario: Pixelated filter
- **WHEN** the user selects "Pixelated" in Preferences
- **THEN** all open sessions render with upstream's `Pixelated` shader on their next frame
