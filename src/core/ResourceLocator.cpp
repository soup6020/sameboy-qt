#include "ResourceLocator.h"
#include "settings/Settings.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace ResourceLocator {

QString dataDirectory()
{
    static const QString directory = [] {
        QStringList candidates;
        const QString env = qEnvironmentVariable("SAMEBOY_QT_DATA_DIR");
        if (!env.isEmpty()) {
            candidates << env;
        }
        const QString appDir = QCoreApplication::applicationDirPath();
        candidates << appDir + QStringLiteral("/share/sameboy-qt")
                   << appDir + QStringLiteral("/../share/sameboy-qt")
                   << appDir + QStringLiteral("/../Resources")
#ifdef SAMEBOY_QT_INSTALL_DATADIR
                   << QStringLiteral(SAMEBOY_QT_INSTALL_DATADIR)
#endif
            ;
        for (const QString &candidate : candidates) {
            if (QFileInfo::exists(candidate + QStringLiteral("/Shaders/MasterShader.fsh"))) {
                return QDir(candidate).canonicalPath();
            }
        }
        qWarning("sameboy-qt: could not locate data directory (set SAMEBOY_QT_DATA_DIR)");
        return appDir;
    }();
    return directory;
}

QString shaderSource(const QString &name)
{
    QFile file(dataDirectory() + QStringLiteral("/Shaders/") + name + QStringLiteral(".fsh"));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

QString registersSymbolFile()
{
    return dataDirectory() + QStringLiteral("/registers.sym");
}

QString bootROMPath(const QString &name)
{
    const QString folder = Settings::instance().stringValue(QStringLiteral("GBBootROMsFolder"));
    if (!folder.isEmpty()) {
        const QString path = folder + QLatin1Char('/') + name + QStringLiteral(".bin");
        if (QFileInfo::exists(path)) {
            return path;
        }
    }
    const QString builtin = dataDirectory() + QStringLiteral("/BootROMs/") + name + QStringLiteral(".bin");
    if (QFileInfo::exists(builtin)) {
        return builtin;
    }
    return {};
}

QString backgroundImagePath()
{
    return dataDirectory() + QStringLiteral("/background.bmp");
}

QString licenseText()
{
    QFile file(dataDirectory() + QStringLiteral("/LICENSE"));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

} // namespace ResourceLocator
