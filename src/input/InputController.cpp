#include "InputController.h"
#include "core/EmulatorSession.h"

#include <QKeyEvent>

#include <cmath>

namespace {

// Workboy tables from Cocoa/GBView.m
uint8_t workboyKeyForCharacter(QChar c)
{
    switch (c.toLower().unicode()) {
        case '0': return GB_WORKBOY_0;
        case '`': return GB_WORKBOY_UMLAUT;
        case '1': return GB_WORKBOY_1;
        case '2': return GB_WORKBOY_2;
        case '3': return GB_WORKBOY_3;
        case '4': return GB_WORKBOY_4;
        case '5': return GB_WORKBOY_5;
        case '6': return GB_WORKBOY_6;
        case '7': return GB_WORKBOY_7;
        case '8': return GB_WORKBOY_8;
        case '9': return GB_WORKBOY_9;
        case '\r': return GB_WORKBOY_ENTER;
        case 3: return GB_WORKBOY_ENTER;
        case '!': return GB_WORKBOY_EXCLAMATION_MARK;
        case '$': return GB_WORKBOY_DOLLAR;
        case '#': return GB_WORKBOY_HASH;
        case '~': return GB_WORKBOY_TILDE;
        case '*': return GB_WORKBOY_ASTERISK;
        case '+': return GB_WORKBOY_PLUS;
        case '-': return GB_WORKBOY_MINUS;
        case '(': return GB_WORKBOY_LEFT_PARENTHESIS;
        case ')': return GB_WORKBOY_RIGHT_PARENTHESIS;
        case ';': return GB_WORKBOY_SEMICOLON;
        case ':': return GB_WORKBOY_COLON;
        case '%': return GB_WORKBOY_PERCENT;
        case '=': return GB_WORKBOY_EQUAL;
        case ',': return GB_WORKBOY_COMMA;
        case '<': return GB_WORKBOY_LT;
        case '.': return GB_WORKBOY_DOT;
        case '>': return GB_WORKBOY_GT;
        case '/': return GB_WORKBOY_SLASH;
        case '?': return GB_WORKBOY_QUESTION_MARK;
        case ' ': return GB_WORKBOY_SPACE;
        case '\'': return GB_WORKBOY_QUOTE;
        case '@': return GB_WORKBOY_AT;
        case 'q': return GB_WORKBOY_Q;
        case 'w': return GB_WORKBOY_W;
        case 'e': return GB_WORKBOY_E;
        case 'r': return GB_WORKBOY_R;
        case 't': return GB_WORKBOY_T;
        case 'y': return GB_WORKBOY_Y;
        case 'u': return GB_WORKBOY_U;
        case 'i': return GB_WORKBOY_I;
        case 'o': return GB_WORKBOY_O;
        case 'p': return GB_WORKBOY_P;
        case 'a': return GB_WORKBOY_A;
        case 's': return GB_WORKBOY_S;
        case 'd': return GB_WORKBOY_D;
        case 'f': return GB_WORKBOY_F;
        case 'g': return GB_WORKBOY_G;
        case 'h': return GB_WORKBOY_H;
        case 'j': return GB_WORKBOY_J;
        case 'k': return GB_WORKBOY_K;
        case 'l': return GB_WORKBOY_L;
        case 'z': return GB_WORKBOY_Z;
        case 'x': return GB_WORKBOY_X;
        case 'c': return GB_WORKBOY_C;
        case 'v': return GB_WORKBOY_V;
        case 'b': return GB_WORKBOY_B;
        case 'n': return GB_WORKBOY_N;
        case 'm': return GB_WORKBOY_M;
    }
    return 0;
}

uint8_t workboyKeyForKey(const QKeyEvent *event)
{
    const bool keypad = event->modifiers() & Qt::KeypadModifier;
    switch (event->key()) {
        case Qt::Key_F1: return GB_WORKBOY_CLOCK;
        case Qt::Key_F2: return GB_WORKBOY_TEMPERATURE;
        case Qt::Key_F3: return GB_WORKBOY_MONEY;
        case Qt::Key_F4: return GB_WORKBOY_CALCULATOR;
        case Qt::Key_F5: return GB_WORKBOY_DATE;
        case Qt::Key_F6: return GB_WORKBOY_CONVERSION;
        case Qt::Key_F7: return GB_WORKBOY_RECORD;
        case Qt::Key_F8: return GB_WORKBOY_WORLD;
        case Qt::Key_F9: return GB_WORKBOY_PHONE;
        case Qt::Key_F10: return GB_WORKBOY_UNKNOWN;
        case Qt::Key_Backspace: return GB_WORKBOY_BACKSPACE;
        case Qt::Key_Shift: return GB_WORKBOY_SHIFT_DOWN;
        case Qt::Key_Up: return GB_WORKBOY_UP;
        case Qt::Key_Down: return GB_WORKBOY_DOWN;
        case Qt::Key_Left: return GB_WORKBOY_LEFT;
        case Qt::Key_Right: return GB_WORKBOY_RIGHT;
        case Qt::Key_Escape: return GB_WORKBOY_ESCAPE;
        case Qt::Key_Clear: return GB_WORKBOY_M;
        case Qt::Key_Enter:
        case Qt::Key_Return: return GB_WORKBOY_ENTER;
    }
    if (keypad) {
        switch (event->key()) {
            case Qt::Key_Period: return GB_WORKBOY_DECIMAL_POINT;
            case Qt::Key_Asterisk: return GB_WORKBOY_H;
            case Qt::Key_Slash: return GB_WORKBOY_J;
        }
    }
    return 0;
}

} // namespace

InputController::InputController(EmulatorSession *session, QObject *parent)
    : QObject(parent), m_session(session)
{
    Settings &settings = Settings::instance();
    connect(&settings, &Settings::changed, this, [this](const QString &key) {
        if (key.startsWith(QLatin1String("GB"))) {
            m_bindingsDirty = true;
        }
    });
    settings.observe(this, QStringLiteral("GBAnalogControls"), [this](const QVariant &value) {
        m_analogControls = value.toBool();
    });
    settings.observe(this, QStringLiteral("JoyKitDefaultControllers"), [this](const QVariant &) {
        reassignControllers();
    });

    GamepadManager &gamepads = GamepadManager::instance();
    connect(&gamepads, &GamepadManager::actionChanged, this, &InputController::onAction);
    connect(&gamepads, &GamepadManager::axisMoved, this, &InputController::onAxis);
    connect(&gamepads, &GamepadManager::leftStickMoved, this, &InputController::onLeftStick);
    connect(&gamepads, &GamepadManager::accelerometer, this, &InputController::onAccelerometer);
    connect(&gamepads, &GamepadManager::controllerConnected, this, &InputController::reassignControllers);
    connect(&gamepads, &GamepadManager::controllerDisconnected, this, &InputController::reassignControllers);

    connect(session, &EmulatorSession::rumble, this, &InputController::setRumble);
    session->setFrameHook([this] { frameHook(); });
    reassignControllers();
}

InputController::~InputController()
{
    if (m_session) {
        m_session->setFrameHook(nullptr);
    }
    setRumble(0);
}

void InputController::rebuildBindings()
{
    m_bindingsDirty = false;
    m_bindings.clear();
    Settings &settings = Settings::instance();
    for (unsigned player = 0; player < 4; player++) {
        const int count = player == 0 ? int(GBButton::KeyboardCount) : int(GBButton::PerPlayerCount);
        for (int button = 0; button < count; button++) {
            const QVariant key = settings.value(buttonPreferenceName(GBButton(button), player));
            if (key.isValid() && key.toInt()) {
                m_bindings[key.toInt()].append({GBButton(button), player});
            }
        }
    }
}

unsigned InputController::playerCount() const
{
    if (!m_session) {
        return 1;
    }
    if (m_session->partner()) {
        return 2;
    }
    if (!GB_is_inited(m_session->gb())) {
        return 1;
    }
    return GB_get_player_count(m_session->gb());
}

// MARK: - Keyboard

void InputController::pressButton(GBButton button, unsigned player, bool pressed)
{
    EmulatorSession *session = m_session;
    GB_gameboy_t *gb = session->gb();
    EmulatorSession *partner = session->partner();
    switch (button) {
        case GBButton::Turbo:
            if (session->isSlave()) {
                GB_set_turbo_mode(partner->gb(), pressed, false);
            }
            else {
                GB_set_turbo_mode(gb, pressed, pressed && session->isRewinding());
            }
            m_turbo = pressed;
            m_analogClockMultiplierValid = false;
            break;
        case GBButton::Rewind:
            if (!pressed) {
                session->setRewinding(false);
            }
            else if (!partner) {
                session->setRewinding(true);
                GB_set_turbo_mode(gb, false, false);
                m_turbo = false;
            }
            break;
        case GBButton::Underclock:
            m_underclockKeyDown = pressed;
            m_analogClockMultiplierValid = false;
            break;
        case GBButton::RapidA:
            m_rapidA[player] = pressed;
            m_rapidACount[player] = 0;
            GB_set_key_state_for_player(gb, GB_KEY_A, player, pressed);
            break;
        case GBButton::RapidB:
            m_rapidB[player] = pressed;
            m_rapidBCount[player] = 0;
            GB_set_key_state_for_player(gb, GB_KEY_B, player, pressed);
            break;
        default: {
            GB_gameboy_t *target = gb;
            unsigned targetPlayer = player;
            if (partner) {
                // With a link cable, player 2's bindings drive the other Game Boy.
                target = player == 0 ? gb : partner->gb();
                targetPlayer = 0;
            }
            GB_set_key_state_for_player(target, GB_key_t(button), targetPlayer, pressed);
            if (pressed && GB_key_t(button) <= GB_KEY_DOWN) {
                GB_set_use_faux_analog_inputs(target, targetPlayer, false);
            }
            break;
        }
    }
}

bool InputController::keyEvent(QKeyEvent *event, bool pressed)
{
    if (!m_session || !GB_is_inited(m_session->gb())) {
        return false;
    }
    if (pressed && event->isAutoRepeat()) {
        return m_bindings.contains(event->key());
    }
    GB_gameboy_t *gb = m_session->gb();

    if (GB_workboy_is_enabled(gb)) {
        if (!pressed) {
            GB_workboy_set_key(gb, event->key() == Qt::Key_Shift ? GB_WORKBOY_SHIFT_UP : GB_WORKBOY_NONE);
        }
        else {
            uint8_t key = workboyKeyForKey(event);
            if (!key && !event->text().isEmpty()) {
                key = workboyKeyForCharacter(event->text().at(0));
            }
            if (key) {
                GB_workboy_set_key(gb, key);
                return true;
            }
        }
    }

    if (m_bindingsDirty) {
        rebuildBindings();
    }
    const auto it = m_bindings.constFind(event->key());
    if (it == m_bindings.constEnd()) {
        return false;
    }
    const unsigned players = playerCount();
    bool handled = false;
    for (const Binding &binding : *it) {
        if (binding.player >= players) {
            continue;
        }
        handled = true;
        pressButton(binding.button, binding.player, pressed);
    }
    if (handled) {
        if (pressed) {
            m_heldKeys.append(event->key());
        }
        else {
            m_heldKeys.removeAll(event->key());
        }
    }
    return handled;
}

void InputController::releaseAll()
{
    // Called when the window loses focus so keys don't get stuck.
    if (!m_session || !GB_is_inited(m_session->gb())) {
        return;
    }
    const QList<int> held = m_heldKeys;
    for (int key : held) {
        QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier);
        keyEvent(&release, false);
    }
    m_heldKeys.clear();
}

// MARK: - Emulation-thread frame hook (GBView -flip)

void InputController::frameHook()
{
    EmulatorSession *session = m_session;
    if (!session) {
        return;
    }
    GB_gameboy_t *gb = session->gb();
    EmulatorSession *partner = session->partner();
    const bool analogValid = m_analogClockMultiplierValid;
    const double analogMultiplier = m_analogClockMultiplier;

    if (analogValid && m_analogControls) {
        m_clockMultiplier = 1.0;
        GB_set_clock_multiplier(gb, analogMultiplier);
        if (partner) {
            GB_set_clock_multiplier(partner->gb(), analogMultiplier);
        }
        if (analogMultiplier == 1.0) {
            m_analogClockMultiplierValid = false;
        }
        if (analogMultiplier < 2.0 && analogMultiplier > 1.0) {
            GB_set_turbo_mode(gb, false, false);
            if (partner) {
                GB_set_turbo_mode(partner->gb(), false, false);
            }
        }
    }
    else {
        if (m_underclockKeyDown && m_clockMultiplier > 0.5) {
            m_clockMultiplier -= 1.0 / 16;
            GB_set_clock_multiplier(gb, m_clockMultiplier);
            if (partner) {
                GB_set_clock_multiplier(partner->gb(), m_clockMultiplier);
            }
        }
        if (!m_underclockKeyDown && m_clockMultiplier < 1.0) {
            m_clockMultiplier += 1.0 / 16;
            GB_set_clock_multiplier(gb, m_clockMultiplier);
            if (partner) {
                GB_set_clock_multiplier(partner->gb(), m_clockMultiplier);
            }
        }
    }
    if ((!analogValid && m_clockMultiplier > 1) || m_turbo || (analogValid && analogMultiplier > 1)) {
        session->showOSD(tr("Fast forwarding…"));
    }
    else if ((!analogValid && m_clockMultiplier < 1) || (analogValid && analogMultiplier < 1)) {
        session->showOSD(tr("Slow motion…"));
    }
    for (unsigned i = GB_get_player_count(gb); i--;) {
        if (i >= 4) {
            continue;
        }
        if (m_rapidA[i]) {
            const uint8_t count = ++m_rapidACount[i];
            GB_set_key_state_for_player(gb, GB_KEY_A, i, !(count & 2));
        }
        if (m_rapidB[i]) {
            const uint8_t count = ++m_rapidBCount[i];
            GB_set_key_state_for_player(gb, GB_KEY_B, i, !(count & 2));
        }
    }
}

// MARK: - Controllers

bool InputController::allowController() const
{
    return m_isActive ? m_isActive() : true;
}

void InputController::reassignControllers()
{
    const unsigned players = playerCount();
    m_lastPlayerCount = players;
    // Don't assign controllers if there's only one player, allow all controllers.
    if (players == 1) {
        m_controllerMapping.clear();
        return;
    }
    GamepadManager &gamepads = GamepadManager::instance();
    for (auto it = m_controllerMapping.begin(); it != m_controllerMapping.end();) {
        if (it.key() >= players || !gamepads.controller(it.value())) {
            it = m_controllerMapping.erase(it);
        }
        else {
            ++it;
        }
    }
    const QVariantMap preferred = Settings::instance().mapValue(QStringLiteral("JoyKitDefaultControllers"));
    for (unsigned i = 0; i < players; i++) {
        const QString preferredId = preferred.value(QString::number(i, 16)).toString();
        if (!preferredId.isEmpty() && gamepads.controller(preferredId)) {
            m_controllerMapping[i] = preferredId;
        }
    }
}

void InputController::tryAssigningController(const QString &uniqueId)
{
    const unsigned players = playerCount();
    if (players == 1 || unsigned(m_controllerMapping.size()) == players) {
        return;
    }
    for (const QString &assigned : m_controllerMapping) {
        if (assigned == uniqueId) {
            return;
        }
    }
    for (unsigned i = 0; i < players; i++) {
        if (!m_controllerMapping.contains(i)) {
            m_controllerMapping[i] = uniqueId;
            return;
        }
    }
}

bool InputController::applicable(const QString &uniqueId, unsigned player, unsigned *effectivePlayer,
                                 GB_gameboy_t **effectiveGB)
{
    if (m_lastPlayerCount != playerCount()) {
        reassignControllers();
    }
    const QString preferred = m_controllerMapping.value(player);
    if (!preferred.isEmpty() && preferred != uniqueId) {
        return false; // The player has a different assigned controller
    }
    if (preferred.isEmpty() && playerCount() != 1) {
        return false; // No assigned controller in multiplayer mode
    }
    GamepadManager::instance().setPlayerIndex(uniqueId, int(player));
    *effectiveGB = m_session->gb();
    *effectivePlayer = player;
    if (player && m_session->partner()) {
        *effectiveGB = m_session->partner()->gb();
        *effectivePlayer = 0;
    }
    if (uniqueId != m_lastController) {
        setRumble(0);
        m_lastController = uniqueId;
    }
    return true;
}

bool InputController::shouldUseJoystickForMotion(const QString &uniqueId) const
{
    if (!m_session || !GB_is_inited(m_session->gb()) || !GB_has_accelerometer(m_session->gb())) {
        return false;
    }
    if (Settings::instance().boolValue(QStringLiteral("GBMBC7JoystickOverride"))) {
        return true;
    }
    const auto *controller = GamepadManager::instance().controller(uniqueId);
    return !controller || !controller->hasAccelerometer;
}

void InputController::onAction(const QString &uniqueId, GamepadAction action, bool pressed, bool fromStick)
{
    if (!m_session || !GB_is_inited(m_session->gb()) || !allowController()) {
        return;
    }
    if (m_mouseControlEnabled) {
        m_mouseControlEnabled = false;
        emit controllerUsed();
    }
    if (fromStick && shouldUseJoystickForMotion(uniqueId)) {
        return;
    }
    tryAssigningController(uniqueId);
    const bool fauxAnalog = Settings::instance().boolValue(QStringLiteral("GBFauxAnalogInputs"));
    const unsigned players = playerCount();

    if (action == GamepadAction::Hotkey1 || action == GamepadAction::Hotkey2) {
        if (pressed) {
            emit hotkey(Settings::instance().stringValue(action == GamepadAction::Hotkey1 ? QStringLiteral("GBJoypadHotkey1")
                                                                                          : QStringLiteral("GBJoypadHotkey2")));
        }
        return;
    }

    for (unsigned player = 0; player < players; player++) {
        unsigned effectivePlayer = 0;
        GB_gameboy_t *effectiveGB = nullptr;
        if (!applicable(uniqueId, player, &effectivePlayer, &effectiveGB)) {
            continue;
        }
        if (action >= GamepadAction::Right && action <= GamepadAction::Down) {
            if (fauxAnalog && fromStick) {
                // Handled as an analog stick instead
                continue;
            }
            // User used a digital direction input, revert to non-analog inputs
            GB_set_use_faux_analog_inputs(effectiveGB, effectivePlayer, false);
        }
        switch (action) {
            case GamepadAction::Right: GB_set_key_state_for_player(effectiveGB, GB_KEY_RIGHT, effectivePlayer, pressed); break;
            case GamepadAction::Left: GB_set_key_state_for_player(effectiveGB, GB_KEY_LEFT, effectivePlayer, pressed); break;
            case GamepadAction::Up: GB_set_key_state_for_player(effectiveGB, GB_KEY_UP, effectivePlayer, pressed); break;
            case GamepadAction::Down: GB_set_key_state_for_player(effectiveGB, GB_KEY_DOWN, effectivePlayer, pressed); break;
            case GamepadAction::A: GB_set_key_state_for_player(effectiveGB, GB_KEY_A, effectivePlayer, pressed); break;
            case GamepadAction::B: GB_set_key_state_for_player(effectiveGB, GB_KEY_B, effectivePlayer, pressed); break;
            case GamepadAction::Select: GB_set_key_state_for_player(effectiveGB, GB_KEY_SELECT, effectivePlayer, pressed); break;
            case GamepadAction::Start: GB_set_key_state_for_player(effectiveGB, GB_KEY_START, effectivePlayer, pressed); break;
            case GamepadAction::RapidA:
                m_rapidA[effectivePlayer] = pressed;
                m_rapidACount[effectivePlayer] = 0;
                GB_set_key_state_for_player(effectiveGB, GB_KEY_A, effectivePlayer, pressed);
                break;
            case GamepadAction::RapidB:
                m_rapidB[effectivePlayer] = pressed;
                m_rapidBCount[effectivePlayer] = 0;
                GB_set_key_state_for_player(effectiveGB, GB_KEY_B, effectivePlayer, pressed);
                break;
            case GamepadAction::Rewind:
                m_session->setRewinding(pressed && !m_session->partner());
                if (pressed) {
                    GB_set_turbo_mode(m_session->isSlave() ? m_session->partner()->gb() : m_session->gb(), false, false);
                    m_turbo = false;
                }
                break;
            case GamepadAction::Turbo:
                if (!m_analogClockMultiplierValid || m_analogClockMultiplier == 1.0 || !pressed) {
                    if (m_session->isSlave()) {
                        GB_set_turbo_mode(m_session->partner()->gb(), pressed, false);
                    }
                    else {
                        GB_set_turbo_mode(m_session->gb(), pressed, pressed && m_session->isRewinding());
                    }
                    m_turbo = pressed;
                }
                break;
            case GamepadAction::Underclock:
                m_underclockKeyDown = pressed;
                break;
            default:
                break;
        }
    }
}

void InputController::onAxis(const QString &uniqueId, const QString &inputId, double value)
{
    if (!m_session || !allowController()) {
        return;
    }
    const auto *controller = GamepadManager::instance().controller(uniqueId);
    if (!controller) {
        return;
    }
    GamepadManager &gamepads = GamepadManager::instance();
    if (inputId == gamepads.analogUnderclockAxis(*controller)) {
        m_analogClockMultiplier = qBound(1.0 / 3, 1 - value + 0.05, 1.0);
        m_analogClockMultiplierValid = true;
    }
    else if (inputId == gamepads.analogTurboAxis(*controller)) {
        m_analogClockMultiplier = qBound(1.0, value * 3 + 0.95, 3.0);
        m_analogClockMultiplierValid = true;
    }
}

void InputController::onLeftStick(const QString &uniqueId, QPointF value)
{
    if (!m_session || !GB_is_inited(m_session->gb()) || !allowController()) {
        return;
    }
    if (shouldUseJoystickForMotion(uniqueId) && !m_mouseControlEnabled) {
        GB_set_accelerometer_values(m_session->gb(), -value.x(), -value.y());
    }
    else if (Settings::instance().boolValue(QStringLiteral("GBFauxAnalogInputs"))) {
        const unsigned players = playerCount();
        for (unsigned player = 0; player < players; player++) {
            unsigned effectivePlayer = 0;
            GB_gameboy_t *effectiveGB = nullptr;
            if (!applicable(uniqueId, player, &effectivePlayer, &effectiveGB)) {
                continue;
            }
            GB_set_use_faux_analog_inputs(effectiveGB, effectivePlayer, true);
            GB_set_faux_analog_inputs(effectiveGB, effectivePlayer, value.x(), value.y());
        }
    }
}

void InputController::onAccelerometer(const QString &uniqueId, QVector3D value)
{
    if (!m_session || !GB_is_inited(m_session->gb()) || !allowController()) {
        return;
    }
    if (Settings::instance().boolValue(QStringLiteral("GBMBC7JoystickOverride")) || m_mouseControlEnabled) {
        return;
    }
    if (uniqueId != m_lastController) {
        return;
    }
    GB_gameboy_t *gb = m_session->gb();
    if (m_session->partner()) {
        if (m_controllerMapping.value(1) == uniqueId) {
            gb = m_session->partner()->gb();
        }
        else if (m_controllerMapping.value(0) != uniqueId) {
            return;
        }
    }
    // SDL: +X right, +Y up, +Z towards the player. JoyKit used x and z.
    GB_set_accelerometer_values(gb, value.x(), value.z());
}

void InputController::setRumble(double amplitude)
{
    if (m_lastController.isEmpty()) {
        return;
    }
    const double strength = Settings::instance().doubleValue(QStringLiteral("GBRumbleStrength"));
    if (strength != 1) {
        amplitude = std::pow(amplitude, strength) * strength;
    }
    GamepadManager::instance().setRumble(m_lastController, amplitude);
}
