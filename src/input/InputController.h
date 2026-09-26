#pragma once

#include <QHash>
#include <QObject>
#include <QPointF>
#include <QPointer>
#include <QVector3D>

#include <atomic>
#include <functional>

#include "GamepadManager.h"
#include "settings/Settings.h"

extern "C" {
#include <Core/gb.h>
}

class EmulatorSession;
class QKeyEvent;

// Per-window input routing: keyboard bindings, controllers, rapid fire,
// turbo/rewind/slow-motion, hotkeys, motion and rumble. Port of the input half
// of Cocoa/GBView.m.
class InputController : public QObject
{
    Q_OBJECT

public:
    explicit InputController(EmulatorSession *session, QObject *parent = nullptr);
    ~InputController() override;

    // Returns true if the key was consumed.
    bool keyEvent(QKeyEvent *event, bool pressed);
    void releaseAll();

    // Whether this window currently receives controller input.
    void setActivePredicate(std::function<bool()> predicate) { m_isActive = std::move(predicate); }
    bool mouseControlEnabled() const { return m_mouseControlEnabled; }
    void setMouseControlEnabled(bool enabled) { m_mouseControlEnabled = enabled; }

signals:
    void hotkey(const QString &action);
    void controllerUsed(); // Motion controls should follow the controller, not the mouse

private:
    struct Binding {
        GBButton button;
        unsigned player;
    };

    void rebuildBindings();
    void frameHook(); // Emulation thread
    unsigned playerCount() const;
    void reassignControllers();
    void tryAssigningController(const QString &uniqueId);
    bool applicable(const QString &uniqueId, unsigned player, unsigned *effectivePlayer, GB_gameboy_t **effectiveGB);
    bool shouldUseJoystickForMotion(const QString &uniqueId) const;
    bool allowController() const;
    void setRumble(double amplitude);

    void pressButton(GBButton button, unsigned player, bool pressed);
    void onAction(const QString &uniqueId, GamepadAction action, bool pressed, bool fromStick);
    void onAxis(const QString &uniqueId, const QString &inputId, double value);
    void onLeftStick(const QString &uniqueId, QPointF value);
    void onAccelerometer(const QString &uniqueId, QVector3D value);

    QPointer<EmulatorSession> m_session;
    std::function<bool()> m_isActive;
    QHash<int, QList<Binding>> m_bindings;
    bool m_bindingsDirty = true;
    QList<int> m_heldKeys;

    // Shared with the emulation thread
    std::atomic<bool> m_turbo{false};
    std::atomic<bool> m_underclockKeyDown{false};
    std::atomic<double> m_analogClockMultiplier{1.0};
    std::atomic<bool> m_analogClockMultiplierValid{false};
    std::atomic<bool> m_analogControls{false};
    std::atomic<bool> m_rapidA[4] = {};
    std::atomic<bool> m_rapidB[4] = {};
    std::atomic<uint8_t> m_rapidACount[4] = {};
    std::atomic<uint8_t> m_rapidBCount[4] = {};
    double m_clockMultiplier = 1.0; // Emulation thread only

    QHash<unsigned, QString> m_controllerMapping;
    unsigned m_lastPlayerCount = 0;
    QString m_lastController;
    bool m_mouseControlEnabled = true;
};
