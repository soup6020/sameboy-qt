#pragma once

#include <QObject>
#include <QSettings>
#include <QVariant>
#include <functional>

// Persistent preferences. Key names and value encodings deliberately mirror the
// Cocoa frontend's NSUserDefaults keys (see third_party/SameBoy/Cocoa/GBApp.m), so
// upstream frontend changes can be ported by grepping for the key name.
class Settings : public QObject
{
    Q_OBJECT

public:
    static Settings &instance();

    QVariant value(const QString &key) const;
    bool boolValue(const QString &key) const { return value(key).toBool(); }
    int intValue(const QString &key) const { return value(key).toInt(); }
    double doubleValue(const QString &key) const { return value(key).toDouble(); }
    QString stringValue(const QString &key) const { return value(key).toString(); }
    QVariantMap mapValue(const QString &key) const { return value(key).toMap(); }

    void setValue(const QString &key, const QVariant &value);
    void remove(const QString &key);
    bool contains(const QString &key) const;
    QVariant defaultValue(const QString &key) const { return m_defaults.value(key); }

    // Invokes |callback| now and again whenever |key| changes, for as long as
    // |context| lives. Equivalent of Cocoa's observeStandardDefaultsKey:withBlock:.
    void observe(QObject *context, const QString &key, const std::function<void(const QVariant &)> &callback,
                 bool callNow = true);

    // Merges built-in palette themes into the user's GBThemes (see GBApp.m).
    void updateThemesDefault(bool overwrite);

    QStringList allKeys() const;
    QString storagePath() const { return m_store.fileName(); }

signals:
    void changed(const QString &key, const QVariant &value);

private:
    Settings();
    void registerDefaults();

    mutable QSettings m_store;
    QVariantMap m_defaults;
};

// Names used for button preferences, identical to GBButtons.m.
enum class GBButton : int {
    Right,
    Left,
    Up,
    Down,
    A,
    B,
    Select,
    Start,
    RapidA,
    RapidB,
    Turbo,
    Rewind,
    Underclock,
    Hotkey1,
    Hotkey2,
    TotalCount,
    KeyboardCount = Underclock + 1,
    PerPlayerCount = RapidB + 1,
};

QString buttonName(GBButton button);
QString buttonPreferenceName(GBButton button, unsigned player);
