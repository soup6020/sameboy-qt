#pragma once

#include <QColor>
#include <QVariantMap>

extern "C" {
#include <Core/gb.h>
}

// Default monochrome palette themes, transcribed from Cocoa/GBApp.m.
QVariantMap defaultPaletteThemes();

// Returns the palette selected by GBColorPalette/GBCurrentTheme
// (port of +[GBPaletteEditorController userPalette]).
const GB_palette_t *currentUserPalette();

// Theme dictionary helpers ("Colors" are 0xAABBGGRR, as stored by Cocoa).
QColor themeColorFromInt(uint32_t c);
uint32_t themeColorToInt(const QColor &color);
