#include "GamepadManager.h"
#include "settings/Settings.h"

#include <SDL3/SDL.h>

#include <cmath>

namespace {

constexpr double kStickDeadzone = 0.35; // For stick-as-d-pad emulation
constexpr double kTriggerThreshold = 0.5;

QString buttonId(int button)
{
    return QStringLiteral("b%1").arg(button);
}

QString axisId(int axis)
{
    return QStringLiteral("a%1").arg(axis);
}

GamepadAction defaultAction(const QString &inputId)
{
    // Default JoyKit usage mapping from GBView.m -controller:buttonChangedState:
    static const QHash<QString, GamepadAction> defaults = {
        {buttonId(SDL_GAMEPAD_BUTTON_DPAD_RIGHT), GamepadAction::Right},
        {buttonId(SDL_GAMEPAD_BUTTON_DPAD_LEFT), GamepadAction::Left},
        {buttonId(SDL_GAMEPAD_BUTTON_DPAD_UP), GamepadAction::Up},
        {buttonId(SDL_GAMEPAD_BUTTON_DPAD_DOWN), GamepadAction::Down},
        {buttonId(SDL_GAMEPAD_BUTTON_SOUTH), GamepadAction::A},
        {buttonId(SDL_GAMEPAD_BUTTON_EAST), GamepadAction::B},
        {buttonId(SDL_GAMEPAD_BUTTON_START), GamepadAction::Start},
        {buttonId(SDL_GAMEPAD_BUTTON_NORTH), GamepadAction::Start},
        {buttonId(SDL_GAMEPAD_BUTTON_BACK), GamepadAction::Select},
        {buttonId(SDL_GAMEPAD_BUTTON_WEST), GamepadAction::Select},
        {buttonId(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER), GamepadAction::Turbo},
        {buttonId(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER), GamepadAction::Underclock},
        {axisId(SDL_GAMEPAD_AXIS_LEFT_TRIGGER), GamepadAction::Rewind},
        {axisId(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER), GamepadAction::Rewind},
    };
    return defaults.value(inputId, GamepadAction::None);
}

} // namespace

GamepadAction gamepadActionForButton(int gbButton)
{
    return GamepadAction(gbButton + 1);
}

GamepadManager &GamepadManager::instance()
{
    static GamepadManager manager;
    return manager;
}

GamepadManager::GamepadManager()
{
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (!SDL_WasInit(SDL_INIT_GAMEPAD) && !SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
        qWarning("sameboy-qt: SDL gamepad init failed: %s", SDL_GetError());
        return;
    }
    int count = 0;
    if (SDL_JoystickID *ids = SDL_GetGamepads(&count)) {
        for (int i = 0; i < count; i++) {
            open(int(ids[i]));
        }
        SDL_free(ids);
    }
    m_timer.setInterval(4);
    connect(&m_timer, &QTimer::timeout, this, &GamepadManager::poll);
    m_timer.start();
}

QList<const GamepadManager::Controller *> GamepadManager::controllers() const
{
    QList<const Controller *> list;
    for (const Controller &controller : m_controllers) {
        list << &controller;
    }
    std::sort(list.begin(), list.end(), [](auto *a, auto *b) { return a->instanceId < b->instanceId; });
    return list;
}

const GamepadManager::Controller *GamepadManager::controller(const QString &uniqueId) const
{
    for (const Controller &controller : m_controllers) {
        if (controller.uniqueId == uniqueId) {
            return &controller;
        }
    }
    return nullptr;
}

void GamepadManager::open(int instanceId)
{
    if (m_controllers.contains(instanceId)) {
        return;
    }
    SDL_Gamepad *gamepad = SDL_OpenGamepad(SDL_JoystickID(instanceId));
    if (!gamepad) {
        return;
    }
    Controller controller;
    controller.instanceId = instanceId;
    controller.gamepad = gamepad;
    controller.name = QString::fromUtf8(SDL_GetGamepadName(gamepad) ?: "Controller");
    char guid[64] = {};
    SDL_GUIDToString(SDL_GetGamepadGUIDForID(SDL_JoystickID(instanceId)), guid, sizeof(guid));
    const char *serial = SDL_GetGamepadSerial(gamepad);
    const char *path = SDL_GetGamepadPath(gamepad);
    controller.uniqueId = QString::fromLatin1(guid) + QLatin1Char('-') +
        QString::fromUtf8(serial && *serial ? serial : (path ? path : ""));
    if (SDL_GamepadHasSensor(gamepad, SDL_SENSOR_ACCEL)) {
        controller.hasAccelerometer = SDL_SetGamepadSensorEnabled(gamepad, SDL_SENSOR_ACCEL, true);
    }
    const SDL_PropertiesID properties = SDL_GetGamepadProperties(gamepad);
    controller.hasRumble = SDL_GetBooleanProperty(properties, SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN, false);
    const QString uniqueId = controller.uniqueId;
    m_controllers.insert(instanceId, controller);
    emit controllerConnected(uniqueId);
}

void GamepadManager::close(int instanceId)
{
    auto it = m_controllers.find(instanceId);
    if (it == m_controllers.end()) {
        return;
    }
    const QString uniqueId = it->uniqueId;
    SDL_CloseGamepad(it->gamepad);
    m_controllers.erase(it);
    emit controllerDisconnected(uniqueId);
}

QVariantMap GamepadManager::mappingFor(const Controller &controller) const
{
    Settings &settings = Settings::instance();
    QVariantMap mapping = settings.mapValue(QStringLiteral("JoyKitInstanceMapping")).value(controller.uniqueId).toMap();
    if (mapping.isEmpty()) {
        mapping = settings.mapValue(QStringLiteral("JoyKitNameMapping")).value(controller.name).toMap();
    }
    return mapping;
}

bool GamepadManager::hasCustomMapping(const Controller &controller) const
{
    return !mappingFor(controller).isEmpty();
}

GamepadAction GamepadManager::actionFor(const Controller &controller, const QString &inputId) const
{
    const QVariantMap mapping = mappingFor(controller);
    if (mapping.isEmpty()) {
        return defaultAction(inputId);
    }
    return GamepadAction(mapping.value(inputId, int(GamepadAction::None)).toInt());
}

QString GamepadManager::analogTurboAxis(const Controller &controller) const
{
    const QVariantMap mapping = mappingFor(controller);
    if (mapping.isEmpty()) {
        return axisId(SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
    }
    return mapping.value(QStringLiteral("AnalogTurbo")).toString();
}

QString GamepadManager::analogUnderclockAxis(const Controller &controller) const
{
    const QVariantMap mapping = mappingFor(controller);
    if (mapping.isEmpty()) {
        return axisId(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
    }
    return mapping.value(QStringLiteral("AnalogUnderclock")).toString();
}

void GamepadManager::setRumble(const QString &uniqueId, double amplitude)
{
    for (Controller &controller : m_controllers) {
        if (controller.uniqueId == uniqueId && controller.hasRumble) {
            const auto strength = uint16_t(qBound(0.0, amplitude, 1.0) * 0xFFFF);
            SDL_RumbleGamepad(controller.gamepad, strength, strength, strength ? 0 : 1);
        }
    }
}

void GamepadManager::setPlayerIndex(const QString &uniqueId, int player)
{
    for (Controller &controller : m_controllers) {
        if (controller.uniqueId == uniqueId) {
            SDL_SetGamepadPlayerIndex(controller.gamepad, player);
        }
    }
}

void GamepadManager::emitInput(Controller &controller, const QString &inputId, bool pressed, double value,
                               bool fromStick)
{
    emit rawInput(controller.uniqueId, inputId, pressed, value);
    const GamepadAction action = actionFor(controller, inputId);
    if (action != GamepadAction::None) {
        emit actionChanged(controller.uniqueId, action, pressed, fromStick);
    }
}

void GamepadManager::poll()
{
    SDL_PumpEvents();
    SDL_Event event;
    while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_EVENT_GAMEPAD_AXIS_MOTION, SDL_EVENT_GAMEPAD_SENSOR_UPDATE) >
           0) {
        switch (event.type) {
            case SDL_EVENT_GAMEPAD_ADDED: open(int(event.gdevice.which)); break;
            case SDL_EVENT_GAMEPAD_REMOVED: close(int(event.gdevice.which)); break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            case SDL_EVENT_GAMEPAD_BUTTON_UP: {
                auto it = m_controllers.find(int(event.gbutton.which));
                if (it == m_controllers.end()) {
                    break;
                }
                emitInput(*it, buttonId(event.gbutton.button), event.gbutton.down, event.gbutton.down ? 1 : 0, false);
                break;
            }
            case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
                auto it = m_controllers.find(int(event.gaxis.which));
                if (it == m_controllers.end()) {
                    break;
                }
                Controller &controller = *it;
                const double value = event.gaxis.value / 32767.0;
                const int axis = event.gaxis.axis;
                emit axisMoved(controller.uniqueId, axisId(axis), value);
                if (axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
                    // Triggers double as buttons (JoyKit L2/R2).
                    bool &state = controller.triggerPressed[axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER];
                    const bool pressed = value > kTriggerThreshold;
                    if (pressed != state) {
                        state = pressed;
                        emitInput(controller, axisId(axis), pressed, value, false);
                    }
                }
                else if (axis == SDL_GAMEPAD_AXIS_LEFTX || axis == SDL_GAMEPAD_AXIS_LEFTY) {
                    if (axis == SDL_GAMEPAD_AXIS_LEFTX) {
                        controller.leftStick.setX(value);
                    }
                    else {
                        controller.leftStick.setY(value);
                    }
                    emit leftStickMoved(controller.uniqueId, controller.leftStick);
                    // The left stick also emulates the d-pad (JoyKit Axes2DEmulateButtons).
                    const QPointF stick = controller.leftStick;
                    const bool directions[4] = {
                        stick.x() > kStickDeadzone,
                        stick.x() < -kStickDeadzone,
                        stick.y() < -kStickDeadzone,
                        stick.y() > kStickDeadzone,
                    };
                    static const int dpad[4] = {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, SDL_GAMEPAD_BUTTON_DPAD_LEFT,
                                                SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_DOWN};
                    for (int i = 0; i < 4; i++) {
                        if (directions[i] != controller.stickDirections[i]) {
                            controller.stickDirections[i] = directions[i];
                            emitInput(controller, buttonId(dpad[i]), directions[i], directions[i], true);
                        }
                    }
                }
                break;
            }
            case SDL_EVENT_GAMEPAD_SENSOR_UPDATE: {
                if (event.gsensor.sensor != SDL_SENSOR_ACCEL) {
                    break;
                }
                auto it = m_controllers.find(int(event.gsensor.which));
                if (it == m_controllers.end()) {
                    break;
                }
                const float *data = event.gsensor.data;
                emit accelerometer(it->uniqueId, QVector3D(data[0], data[1], data[2]) / SDL_STANDARD_GRAVITY);
                break;
            }
            default: break;
        }
    }
    // Drop anything else SDL queued so the queue doesn't grow.
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
}
