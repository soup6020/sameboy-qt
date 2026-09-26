#include "Settings.h"
#include "PaletteThemes.h"

#include <QFontDatabase>
#include <QFontInfo>
#include <QPointer>

extern "C" {
#include <Core/gb.h>
}

#include "core/Models.h"

static const char *const kButtonNames[] = {
    "Right",   "Left",    "Up",    "Down",   "A",           "B",        "Select",   "Start",
    "Rapid A", "Rapid B", "Turbo", "Rewind", "Slow-Motion", "Hotkey 1", "Hotkey 2",
};
static_assert(sizeof(kButtonNames) / sizeof(kButtonNames[0]) == int(GBButton::TotalCount));

QString buttonName(GBButton button)
{
    return QString::fromLatin1(kButtonNames[int(button)]);
}

QString buttonPreferenceName(GBButton button, unsigned player)
{
    if (player) {
        return QStringLiteral("GBPlayer%1%2").arg(player + 1).arg(buttonName(button));
    }
    return QStringLiteral("GB") + buttonName(button);
}

static QString defaultMonospaceFamily()
{
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    if (!QFontInfo(font).fixedPitch()) {
        font = QFont(QStringLiteral("monospace"));
        font.setStyleHint(QFont::TypeWriter);
    }
    return QFontInfo(font).family();
}

Settings &Settings::instance()
{
    static Settings settings;
    return settings;
}

Settings::Settings()
    // ~/.config/sameboy-qt/sameboy-qt.conf on Linux (native store elsewhere).
    : m_store(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("sameboy-qt"), QStringLiteral("sameboy-qt"))
{
    // One-time migration from the location used by early builds.
    if (m_store.allKeys().isEmpty()) {
        QSettings legacy(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("SameBoy"),
                         QStringLiteral("SameBoy-Qt"));
        for (const QString &key : legacy.allKeys()) {
            m_store.setValue(key, legacy.value(key));
        }
    }
    registerDefaults();
    if (m_store.value(QStringLiteral("GBThemesVersion")).toString() != QStringLiteral(GB_VERSION)) {
        updateThemesDefault(false);
        m_store.setValue(QStringLiteral("GBThemesVersion"), QStringLiteral(GB_VERSION));
    }
}

void Settings::registerDefaults()
{
    QVariantMap &d = m_defaults;

    // Keyboard defaults (Cocoa uses Mac virtual key codes; we use Qt::Key values).
    d[buttonPreferenceName(GBButton::Right, 0)] = int(Qt::Key_Right);
    d[buttonPreferenceName(GBButton::Left, 0)] = int(Qt::Key_Left);
    d[buttonPreferenceName(GBButton::Up, 0)] = int(Qt::Key_Up);
    d[buttonPreferenceName(GBButton::Down, 0)] = int(Qt::Key_Down);
    d[buttonPreferenceName(GBButton::A, 0)] = int(Qt::Key_X);
    d[buttonPreferenceName(GBButton::B, 0)] = int(Qt::Key_Z);
    d[buttonPreferenceName(GBButton::Select, 0)] = int(Qt::Key_Backspace);
    d[buttonPreferenceName(GBButton::Start, 0)] = int(Qt::Key_Return);
    d[buttonPreferenceName(GBButton::Turbo, 0)] = int(Qt::Key_Space);
    d[buttonPreferenceName(GBButton::Rewind, 0)] = int(Qt::Key_Tab);
    d[buttonPreferenceName(GBButton::Underclock, 0)] = int(Qt::Key_Shift);

    d["GBFilter"] = QStringLiteral("NearestNeighbor");
    d["GBColorCorrection"] = int(GB_COLOR_CORRECTION_MODERN_BALANCED);
    d["GBHighpassFilter"] = int(GB_HIGHPASS_ACCURATE);
    d["GBRewindLength"] = 120;
    d["GBFrameBlendingMode"] = int(GB_FRAME_BLENDING_MODE_ACCURATE);

    d["GBDMGModel"] = int(GB_MODEL_DMG_B);
    d["GBCGBModel"] = int(GB_MODEL_CGB_E);
    d["GBAGBModel"] = int(GB_MODEL_AGB_A);
    d["GBSGBModel"] = int(GB_MODEL_SGB2);
    d["GBRumbleMode"] = int(GB_RUMBLE_CARTRIDGE_ONLY);

    d["GBVolume"] = 1.0;

    d["GBMBC7JoystickOverride"] = false;
    d["GBMBC7AllowMouse"] = true;

    d["GBEmulatedModel"] = int(EmulatedModel::Auto);

    d["GBDebuggerFont"] = defaultMonospaceFamily();
    d["GBDebuggerFontSize"] = 12;

    d["GBColorPalette"] = 1;
    d["GBTurboCap"] = 0.0;
    d["GBRumbleStrength"] = 1.0;

    d["GBThemes"] = defaultPaletteThemes();

    // Keys that Cocoa leaves unregistered (implicitly NO / 0 / nil).
    d["GBBorderMode"] = int(GB_BORDER_SGB);
    d["GBRTCMode"] = int(GB_RTC_MODE_SYNC_TO_HOST);
    d["GBLightTemperature"] = 0.0;
    d["GBInterferenceVolume"] = 0.0;
    d["GBAspectRatioUnkept"] = false;
    d["GBForceIntegerScale"] = false;
    d["GBOSDEnabled"] = false;
    d["GBFilterScreenshots"] = false;
    d["GBAnalogControls"] = false;
    d["GBFauxAnalogInputs"] = false;
    d["GBAllowBackgroundControllers"] = false;
    d["GBJoypadHotkey1"] = QString();
    d["GBJoypadHotkey2"] = QString();
    d["Mute"] = false;
    d["DeveloperMode"] = false;
    d["GBWorkboyTimeOffset"] = 0;
}

QVariant Settings::value(const QString &key) const
{
    if (m_store.contains(key)) {
        return m_store.value(key);
    }
    return m_defaults.value(key);
}

bool Settings::contains(const QString &key) const
{
    return m_store.contains(key);
}

void Settings::setValue(const QString &key, const QVariant &newValue)
{
    if (m_store.contains(key) && m_store.value(key) == newValue) {
        return;
    }
    if (!newValue.isValid()) {
        remove(key);
        return;
    }
    m_store.setValue(key, newValue);
    emit changed(key, value(key));
}

void Settings::remove(const QString &key)
{
    if (!m_store.contains(key)) {
        return;
    }
    m_store.remove(key);
    emit changed(key, value(key));
}

void Settings::observe(QObject *context, const QString &key, const std::function<void(const QVariant &)> &callback,
                       bool callNow)
{
    if (callNow) {
        callback(value(key));
    }
    connect(this, &Settings::changed, context, [key, callback](const QString &changedKey, const QVariant &newValue) {
        if (changedKey == key) {
            callback(newValue);
        }
    });
}

void Settings::updateThemesDefault(bool overwrite)
{
    QVariantMap current = m_store.value(QStringLiteral("GBThemes")).toMap();
    QVariantMap defaults = defaultPaletteThemes();
    if (!m_store.contains(QStringLiteral("GBThemesVersion"))) {
        // Force update the Pink Pop theme, it was glitchy in 1.0
        current.remove(QStringLiteral("Pink Pop"));
    }
    if (overwrite) {
        for (auto it = defaults.cbegin(); it != defaults.cend(); ++it) {
            current[it.key()] = it.value();
        }
        setValue(QStringLiteral("GBThemes"), current);
    }
    else {
        for (auto it = current.cbegin(); it != current.cend(); ++it) {
            defaults[it.key()] = it.value();
        }
        setValue(QStringLiteral("GBThemes"), defaults);
    }
}

QStringList Settings::allKeys() const
{
    QStringList keys = m_defaults.keys();
    for (const QString &key : m_store.allKeys()) {
        if (!keys.contains(key)) {
            keys << key;
        }
    }
    keys.sort();
    return keys;
}
