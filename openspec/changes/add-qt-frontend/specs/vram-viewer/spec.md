# Spec Delta

## Purpose

Defines the VRAM viewer for inspecting tiles, tilemaps, objects and palettes during emulation, equivalent to the Cocoa VRAM Viewer.

## ADDED Requirements

### Requirement: Tileset tab
The Tileset tab SHALL show the 256×192 tileset image (both VRAM banks on CGB) with a palette selector: None, Background Palette 0–7 or Object Palette 0–7. It SHALL have an optional grid (8 px lines, plus stronger 128-px horizontal and 64-px vertical lines).

#### Scenario: Hover tile
- **WHEN** the mouse is over the tile at column 1, row 0 of bank 1
- **THEN** the status reads "Tile number $01 at 1:$8010"

### Requirement: Tilemap tab
The Tilemap tab SHALL show the 256×256 background map. It SHALL have selectors for palette (None, Effective Palettes, BG/OBJ 0–7), map (Effective, $9800, $9C00) and tileset (Effective, $8800, $8000). It SHALL have an optional 8-px grid and an optional overlay of the 160×144 scroll viewport at (SCX, SCY) with wrap-around. Hovering SHALL show the tile number, tile address, map address and CGB attributes.

#### Scenario: Scroll rectangle
- **WHEN** Scrolling is enabled and SCX=250
- **THEN** the viewport rectangle wraps horizontally across the image edge

### Requirement: Objects tab
The Objects tab SHALL list all 40 OAM entries from the core in a grid. Each entry SHALL show:
- the object image
- OAM address, position (x−8, y−16) and tile index
- tile address
- attributes: P/Y/X flags plus bank and palette on CGB, or DMG palette number
- a warning marker for objects dropped by the per-line limit

#### Scenario: Dropped object
- **WHEN** 11 objects share a scanline
- **THEN** the 11th shows the "Dropped: too many objects in line" warning

### Requirement: Palettes tab
The Palettes tab SHALL list background palettes 0–7 and object palettes 0–7, each with four swatches. Each swatch SHALL be labelled with its RGB555 hex value in a contrasting text color, and SHALL have a tooltip with its red, green and blue components.

#### Scenario: Palette swatch
- **WHEN** BG palette 0 color 0 is $7FFF
- **THEN** the swatch is white with black "$7FFF" text

### Requirement: Live refresh
The visible tab SHALL refresh every emitted frame while the window is visible, and after debugger output.

#### Scenario: Animation
- **WHEN** a game animates tiles
- **THEN** the Tileset tab reflects the change on the next frame
