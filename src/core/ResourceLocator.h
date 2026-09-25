#pragma once

#include <QString>

// Finds runtime assets staged from the upstream submodule (boot ROMs, shaders,
// registers.sym). Searches, in order: $SAMEBOY_QT_DATA_DIR, <exe>/share/sameboy-qt,
// <exe>/../share/sameboy-qt, the compile-time install prefix.
namespace ResourceLocator {

QString dataDirectory();
QString shaderSource(const QString &name); // Contents of Shaders/<name>.fsh, empty if missing
QString registersSymbolFile();
QString bootROMPath(const QString &name);  // Honors the GBBootROMsFolder preference
QString licenseText();
QString backgroundImagePath(); // SDL/background.bmp: 160x144, 4-colour indexed

} // namespace ResourceLocator
