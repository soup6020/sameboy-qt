#pragma once

#include <QHash>
#include <QObject>
#include <QPointF>
#include <QTimer>
#include <QVariantMap>
#include <QVector3D>

struct SDL_Gamepad;

// Actions a controller input can be mapped to. Mirrors the JoyKit usages the
// Cocoa frontend understands (GBView.m / GBPreferencesWindow.m).
enum class GamepadAction : int {
    None = 0,
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
};

GamepadAction gamepadActionForButton(int gbButton); // GBButton index -> action

// Owns SDL gamepads: hotplug, polling, mapping lookup, rumble, LEDs, sensors.
// Mappings are stored like JoyKit's: JoyKitInstanceMapping[uniqueID] and
// JoyKitNameMapping[deviceName], each {"<input id>": action}. Input ids are
// "b<SDL_GamepadButton>" and "a<SDL_GamepadAxis>" (triggers as buttons).
class GamepadManager : public QObject
{
    Q_OBJECT

public:
    struct Controller {
        int instanceId = 0;
        QString uniqueId;
        QString name;
        SDL_Gamepad *gamepad = nullptr;
        bool hasAccelerometer = false;
        bool hasRumble = false;
        QPointF leftStick;
        bool triggerPressed[2] = {false, false};
        bool stickDirections[4] = {false, false, false, false}; // emulated d-pad from stick
    };

    static GamepadManager &instance();

    QList<const Controller *> controllers() const;
    const Controller *controller(const QString &uniqueId) const;

    void setRumble(const QString &uniqueId, double amplitude);
    void setPlayerIndex(const QString &uniqueId, int player);

    // Returns the mapped action for |inputId| on |controller|, or the default
    // mapping when the controller has none configured.
    GamepadAction actionFor(const Controller &controller, const QString &inputId) const;
    bool hasCustomMapping(const Controller &controller) const;

    // Binding editor support. A mapping is {inputId: int(GamepadAction)} plus
    // optional "AnalogTurbo"/"AnalogUnderclock" axis ids.
    static QVariantMap defaultMapping();
    static QVariantMap storedMapping(const QString &uniqueId, const QString &name); // Empty if none
    static QVariantMap mappingForEditing(const QString &uniqueId, const QString &name); // Stored, else defaults
    static void setMapping(const QString &uniqueId, const QString &name, const QVariantMap &mapping);
    static void resetMapping(const QString &uniqueId, const QString &name);
    static QStringList inputsForAction(const QVariantMap &mapping, GamepadAction action);
    static QVariantMap bindInput(QVariantMap mapping, const QString &inputId, GamepadAction action);
    static QVariantMap clearAction(QVariantMap mapping, GamepadAction action);
    static QString genericInputName(const QString &inputId);
    QString inputDisplayName(const QString &uniqueId, const QString &inputId) const;
    QString analogTurboAxis(const Controller &controller) const;
    QString analogUnderclockAxis(const Controller &controller) const;

signals:
    void controllerConnected(const QString &uniqueId);
    void controllerDisconnected(const QString &uniqueId);
    // Raw input, before mapping (used by the configuration wizard).
    void rawInput(const QString &uniqueId, const QString &inputId, bool pressed, double value);
    // Mapped input.
    void actionChanged(const QString &uniqueId, GamepadAction action, bool pressed, bool fromStick);
    void axisMoved(const QString &uniqueId, const QString &inputId, double value);
    void leftStickMoved(const QString &uniqueId, QPointF value);
    void accelerometer(const QString &uniqueId, QVector3D gUnits);

private:
    GamepadManager();
    void poll();
    void open(int instanceId);
    void close(int instanceId);
    QVariantMap mappingFor(const Controller &controller) const;
    void emitInput(Controller &controller, const QString &inputId, bool pressed, double value, bool fromStick);

    QTimer m_timer;
    QHash<int, Controller> m_controllers;
};
