// Automated checks for the emulation layer, run against a tiny RGBDS-built ROM
// (tests/testrom.asm). They exercise the same code paths the UI uses.

#include "core/EmulatorSession.h"
#include "input/GamepadManager.h"
#include "input/InputController.h"
#include "settings/PaletteThemes.h"
#include "settings/Settings.h"
#include "ui/MainWindow.h"
#include "ui/MemoryModel.h"
#include "ui/PreferencesDialog.h"

#include <QComboBox>
#include <QFile>
#include <QKeyEvent>
#include <QPointer>
#include <QPushButton>
#include <QScopeGuard>
#include <QScrollArea>
#include <QScrollBar>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QtTest>

#include <SDL3/SDL.h>

#include <memory>

namespace {

uint8_t readByte(EmulatorSession *session, uint16_t address)
{
    uint8_t value = 0;
    session->performAtomic([&] { value = GB_safe_read_memory(session->gb(), address); });
    return value;
}

} // namespace

class SameBoyTests : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_directory;
    QString m_rom;

    std::unique_ptr<EmulatorSession> openSession(const QString &name = QStringLiteral("test.gb"))
    {
        const QString path = m_directory.filePath(name);
        if (!QFile::exists(path)) {
            QFile::copy(m_rom, path);
        }
        auto session = std::make_unique<EmulatorSession>(path);
        QString error;
        if (!session->open(&error)) {
            qWarning("open failed: %s", qPrintable(error));
            return nullptr;
        }
        return session;
    }

    // RAM starts out randomized and the boot ROM runs first, so wait until the
    // CPU is in the test ROM's main loop and its SRAM marker has been written.
    void waitForGame(EmulatorSession *session)
    {
        QTRY_VERIFY_WITH_TIMEOUT(
            [&] {
                uint16_t pc = 0;
                session->performAtomic([&] { pc = GB_get_registers(session->gb())->pc; });
                return pc >= 0x150 && pc < 0x200 && readByte(session, 0xA000) == 0x42;
            }(),
            10000);
        // Let the main loop store the joypad state at least once.
        const uint8_t frame = readByte(session, 0xC000);
        QTRY_VERIFY(readByte(session, 0xC000) != frame);
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_directory.isValid());
        qputenv("XDG_CONFIG_HOME", m_directory.filePath(QStringLiteral("config")).toUtf8());
        qputenv("SAMEBOY_QT_DATA_DIR", TEST_DATA_DIR);
        qputenv("SAMEBOY_QT_SOFTWARE_RENDERER", "1"); // The offscreen platform has no OpenGL
        m_rom = QStringLiteral(TEST_ROM);
        QVERIFY(QFile::exists(m_rom));
        QVERIFY(SDL_Init(SDL_INIT_AUDIO));
    }

    void cleanupTestCase() { SDL_Quit(); }

    void settingsDefaults()
    {
        Settings &settings = Settings::instance();
        QCOMPARE(settings.intValue(QStringLiteral("GBColorCorrection")), int(GB_COLOR_CORRECTION_MODERN_BALANCED));
        QCOMPARE(settings.intValue(QStringLiteral("GBCGBModel")), int(GB_MODEL_CGB_E));
        QCOMPARE(settings.intValue(QStringLiteral("GBSGBModel")), int(GB_MODEL_SGB2));
        QCOMPARE(settings.stringValue(QStringLiteral("GBFilter")), QStringLiteral("NearestNeighbor"));
        QCOMPARE(settings.intValue(QStringLiteral("GBRewindLength")), 120);
        QCOMPARE(settings.intValue(QStringLiteral("GBEmulatedModel")), int(EmulatedModel::Auto));
        QCOMPARE(settings.intValue(buttonPreferenceName(GBButton::A, 0)), int(Qt::Key_X));
        QCOMPARE(buttonPreferenceName(GBButton::Underclock, 1), QStringLiteral("GBPlayer2Slow-Motion"));
        const QVariantMap themes = settings.mapValue(QStringLiteral("GBThemes"));
        QCOMPARE(themes.size(), 14);
        QVERIFY(themes.contains(QStringLiteral("Canyon")));
    }

    void settingsLocation()
    {
#ifdef Q_OS_LINUX
        QCOMPARE(Settings::instance().storagePath(),
                 m_directory.filePath(QStringLiteral("config/sameboy-qt/sameboy-qt.conf")));
#endif
    }

    void controllerMappingModel()
    {
        auto b = [](int button) { return QStringLiteral("b%1").arg(button); };
        auto a = [](int axis) { return QStringLiteral("a%1").arg(axis); };
        const QVariantMap defaults = GamepadManager::defaultMapping();

        // Defaults: Start has two inputs, both triggers rewind, analog axes are set.
        QCOMPARE(GamepadManager::inputsForAction(defaults, GamepadAction::Start),
                 QStringList({b(SDL_GAMEPAD_BUTTON_NORTH), b(SDL_GAMEPAD_BUTTON_START)}));
        QCOMPARE(GamepadManager::inputsForAction(defaults, GamepadAction::Rewind),
                 QStringList({a(SDL_GAMEPAD_AXIS_LEFT_TRIGGER), a(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)}));

        // Rebinding replaces the action's inputs and moves the input from its old action.
        QVariantMap mapping = GamepadManager::bindInput(defaults, b(SDL_GAMEPAD_BUTTON_SOUTH), GamepadAction::Start);
        QCOMPARE(GamepadManager::inputsForAction(mapping, GamepadAction::Start),
                 QStringList({b(SDL_GAMEPAD_BUTTON_SOUTH)}));
        QVERIFY(GamepadManager::inputsForAction(mapping, GamepadAction::A).isEmpty());
        QCOMPARE(GamepadManager::inputsForAction(mapping, GamepadAction::B), QStringList({b(SDL_GAMEPAD_BUTTON_EAST)}));

        // A trigger bound to Turbo leaves Rewind and becomes the analog turbo axis.
        mapping = GamepadManager::bindInput(mapping, a(SDL_GAMEPAD_AXIS_LEFT_TRIGGER), GamepadAction::Turbo);
        QCOMPARE(mapping.value(QStringLiteral("AnalogTurbo")).toString(), a(SDL_GAMEPAD_AXIS_LEFT_TRIGGER));
        QCOMPARE(GamepadManager::inputsForAction(mapping, GamepadAction::Rewind),
                 QStringList({a(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)}));

        // Moving that trigger elsewhere drops the stale analog axis.
        mapping = GamepadManager::bindInput(mapping, a(SDL_GAMEPAD_AXIS_LEFT_TRIGGER), GamepadAction::Rewind);
        QVERIFY(!mapping.contains(QStringLiteral("AnalogTurbo")));

        // Clearing removes every input of the action (and its analog axis).
        mapping = GamepadManager::clearAction(mapping, GamepadAction::Underclock);
        QVERIFY(GamepadManager::inputsForAction(mapping, GamepadAction::Underclock).isEmpty());
        QVERIFY(!mapping.contains(QStringLiteral("AnalogUnderclock")));

        // Storage round trip, then reset back to defaults.
        const QString id = QStringLiteral("test-guid-serial"), name = QStringLiteral("Test Pad");
        QVERIFY(GamepadManager::storedMapping(id, name).isEmpty());
        QCOMPARE(GamepadManager::mappingForEditing(id, name), defaults);
        GamepadManager::setMapping(id, name, mapping);
        QCOMPARE(GamepadManager::storedMapping(id, name), mapping);
        QCOMPARE(GamepadManager::storedMapping(QStringLiteral("other-instance"), name), mapping); // Name fallback
        GamepadManager::resetMapping(id, name);
        QCOMPARE(GamepadManager::mappingForEditing(id, name), defaults);

        QCOMPARE(GamepadManager::genericInputName(b(SDL_GAMEPAD_BUTTON_DPAD_UP)), QStringLiteral("D-pad Up"));
        QCOMPARE(GamepadManager::genericInputName(a(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)), QStringLiteral("Right Trigger"));
    }

    // End to end with an SDL virtual gamepad: default mapping reaches the
    // game-facing signal, and the Controls table rebinds a single button.
    void virtualControllerBinding()
    {
        GamepadManager &gamepads = GamepadManager::instance();
        SDL_VirtualJoystickDesc desc;
        SDL_INIT_INTERFACE(&desc);
        desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
        desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
        desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
        desc.name = "SameBoy Test Pad";
        const SDL_JoystickID id = SDL_AttachVirtualJoystick(&desc);
        QVERIFY2(id, SDL_GetError());
        SDL_Joystick *joystick = SDL_OpenJoystick(id);
        QVERIFY(joystick);
        QTRY_COMPARE(gamepads.controllers().size(), 1);
        const QString uniqueId = gamepads.controllers().first()->uniqueId;

        auto press = [&](SDL_GamepadButton button) {
            SDL_SetJoystickVirtualButton(joystick, button, true);
            QTest::qWait(30);
            SDL_SetJoystickVirtualButton(joystick, button, false);
            QTest::qWait(30);
        };

        // Default mapping: South = A.
        QSignalSpy actions(&gamepads, &GamepadManager::actionChanged);
        press(SDL_GAMEPAD_BUTTON_SOUTH);
        QTRY_VERIFY(!actions.isEmpty());
        QCOMPARE(actions.first().at(1).value<GamepadAction>(), GamepadAction::A);
        QVERIFY(actions.first().at(2).toBool());

        // The Controls table shows the controller and its default bindings.
        PreferencesDialog dialog;
        auto *table = dialog.findChild<QTableWidget *>();
        const auto selectors = dialog.findChildren<QComboBox *>();
        QVERIFY(table);
        const auto controllerSelector = std::find_if(selectors.begin(), selectors.end(), [&](QComboBox *combo) {
            return combo->currentData().toString() == uniqueId;
        });
        QVERIFY(controllerSelector != selectors.end());
        const int rowB = int(GBButton::B);
        QVERIFY(!table->item(rowB, 2)->text().isEmpty());

        // Rebind B to West by double-clicking its controller cell and pressing West.
        QTest::qWait(300); // Past the capture debounce
        emit table->cellDoubleClicked(rowB, 2);
        QTRY_VERIFY(table->item(rowB, 2)->text().contains(QStringLiteral("Press")));
        press(SDL_GAMEPAD_BUTTON_WEST);
        const QString west = QStringLiteral("b%1").arg(SDL_GAMEPAD_BUTTON_WEST);
        QTRY_COMPARE(GamepadManager::storedMapping(uniqueId, QString()).value(west).toInt(), int(GamepadAction::B));
        QTRY_COMPARE(table->item(rowB, 2)->text(), gamepads.inputDisplayName(uniqueId, west));
        // Everything else kept its default (South still presses A).
        QCOMPARE(GamepadManager::inputsForAction(GamepadManager::storedMapping(uniqueId, QString()), GamepadAction::A),
                 QStringList({QStringLiteral("b%1").arg(SDL_GAMEPAD_BUTTON_SOUTH)}));

        // The wizard shows each binding live and highlights the next action.
        auto *configure = [&]() -> QPushButton * {
            for (QPushButton *button : dialog.findChildren<QPushButton *>()) {
                if (button->text() == QStringLiteral("Configure a controller")) {
                    return button;
                }
            }
            return nullptr;
        }();
        QVERIFY(configure);
        QTest::qWait(300);
        configure->click();
        QVERIFY(table->item(int(GBButton::Right), 0)->background().style() != Qt::NoBrush);
        press(SDL_GAMEPAD_BUTTON_NORTH); // Bound to Right (step 1)
        QTRY_COMPARE(table->item(int(GBButton::Right), 2)->text(),
                     gamepads.inputDisplayName(uniqueId, QStringLiteral("b%1").arg(SDL_GAMEPAD_BUTTON_NORTH)));
        QVERIFY(table->item(int(GBButton::Left), 0)->background().style() != Qt::NoBrush); // Step 2 highlighted

        dialog.close(); // Stops the wizard
        GamepadManager::resetMapping(uniqueId, gamepads.controllers().first()->name);
        SDL_CloseJoystick(joystick);
        SDL_DetachVirtualJoystick(id);
        QTRY_COMPARE(gamepads.controllers().size(), 0);
    }

    void pauseAndMuteWhenInactive()
    {
        Settings &settings = Settings::instance();
        for (const char *name : {"focusA.gb", "focusB.gb"}) {
            if (!QFile::exists(m_directory.filePath(QString::fromLatin1(name)))) {
                QFile::copy(m_rom, m_directory.filePath(QString::fromLatin1(name)));
            }
        }
        // Deleted by the guard even if a check fails early (MainWindow is WA_DeleteOnClose).
        QPointer<MainWindow> a = new MainWindow(m_directory.filePath(QStringLiteral("focusA.gb")));
        QPointer<MainWindow> b = new MainWindow(m_directory.filePath(QStringLiteral("focusB.gb")));
        const auto cleanup = qScopeGuard([&] {
            delete a.data();
            delete b.data();
        });
        QVERIFY(a->open() && b->open());
        a->show();
        b->show();
        auto focus = [](QWidget *window) {
            window->activateWindow();
            QTRY_COMPARE(QApplication::activeWindow(), window);
            QTest::qWait(250); // Past the focus debounce
        };

        // Off by default: switching windows changes nothing.
        focus(a);
        focus(b);
        QVERIFY(!a->session()->isPaused());
        QVERIFY(a->session()->isAudioPlaying());

        // Mute when inactive: silent while b is focused, persistent Mute untouched.
        settings.setValue(QStringLiteral("GBMuteWhenInactive"), true);
        QTRY_VERIFY(!a->session()->isAudioPlaying());
        QVERIFY(!a->session()->isMuted());
        QVERIFY(!settings.boolValue(QStringLiteral("Mute")));
        focus(a);
        QTRY_VERIFY(a->session()->isAudioPlaying());
        settings.setValue(QStringLiteral("GBMuteWhenInactive"), false);

        // Pause when inactive: pauses on focus loss, resumes on return.
        settings.setValue(QStringLiteral("GBPauseWhenInactive"), true);
        focus(b);
        QTRY_VERIFY(a->session()->isPaused());
        QVERIFY(!b->session()->isPaused());
        focus(a);
        QTRY_VERIFY(!a->session()->isPaused());

        // A game the user paused stays paused.
        a->session()->togglePause();
        QVERIFY(a->session()->isPaused());
        focus(b);
        focus(a);
        QVERIFY(a->session()->isPaused());

        settings.setValue(QStringLiteral("GBPauseWhenInactive"), false);
    }

    // Tiling window managers can force the dialog below its minimum size; the
    // tabs must scroll rather than squash their widgets.
    void preferencesScrollWhenSmall()
    {
        PreferencesDialog dialog;
        dialog.setCurrentTab(PreferencesDialog::ControlsTab);
        dialog.setMinimumSize(0, 0);
        dialog.resize(400, 300);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        auto *table = dialog.findChild<QTableWidget *>();
        QVERIFY(table);
        QTRY_VERIFY(dialog.height() <= 320);
        // The Controls page keeps its natural size inside a scroll area…
        const auto scrollAreas = dialog.findChildren<QScrollArea *>();
        const auto found = std::find_if(scrollAreas.begin(), scrollAreas.end(),
                                        [&](QScrollArea *area) { return area->isAncestorOf(table); });
        if (found == scrollAreas.end() || !(*found)->widget()) {
            QFAIL("The Controls tab is not inside a scroll area");
        }
        QScrollArea *scroll = *found;
        const QWidget *page = scroll->widget();
        QVERIFY(page->height() >= page->minimumSizeHint().height());
        // …and the overflow becomes scrollable instead of overlapping.
        QVERIFY(scroll->verticalScrollBar()->maximum() > 0);
        QVERIFY(table->height() >= table->minimumHeight());
    }

    void settingsObserve()
    {
        Settings &settings = Settings::instance();
        QObject context;
        QList<QVariant> seen;
        settings.observe(&context, QStringLiteral("GBTestKey"), [&](const QVariant &value) { seen << value; });
        settings.setValue(QStringLiteral("GBTestKey"), 5);
        settings.setValue(QStringLiteral("GBTestKey"), 5); // No change, no notification
        settings.remove(QStringLiteral("GBTestKey"));
        QCOMPARE(seen.size(), 3); // Initial (invalid), set, removed
        QCOMPARE(seen[1].toInt(), 5);
    }

    void customPalette()
    {
        Settings &settings = Settings::instance();
        settings.setValue(QStringLiteral("GBColorPalette"), -1);
        settings.setValue(QStringLiteral("GBCurrentTheme"), QStringLiteral("Canyon"));
        const GB_palette_t *palette = currentUserPalette();
        // Canyon's first colour is 0xff0c1e20 (ABGR)
        QCOMPARE(int(palette->colors[0].r), 0x20);
        QCOMPARE(int(palette->colors[0].g), 0x1e);
        QCOMPARE(int(palette->colors[0].b), 0x0c);
        settings.setValue(QStringLiteral("GBColorPalette"), 1);
        QCOMPARE(currentUserPalette(), &GB_PALETTE_DMG);
    }

    void runsAndPicksModel()
    {
        auto session = openSession();
        QVERIFY(session);
        waitForGame(session.get());
        // No CGB/SGB flags and no Nintendo licensee: automatic mode picks DMG.
        QCOMPARE(session->currentModel(), EmulatedModel::DMG);
        QVERIFY(session->usesAutoModel());
        QCOMPARE(session->screenWidth(), 160u);
        QCOMPARE(session->currentFrameImage().size(), QSize(160, 144));
    }

    void explicitModelAndBorder()
    {
        auto session = openSession();
        QVERIFY(session);
        session->reset(EmulatedModel::SGB);
        QVERIFY(GB_is_sgb(session->gb()));
        QCOMPARE(session->screenWidth(), 256u); // SGB border by default
        Settings::instance().setValue(QStringLiteral("GBBorderMode"), int(GB_BORDER_NEVER));
        QTRY_COMPARE(session->screenWidth(), 160u);
        Settings::instance().setValue(QStringLiteral("GBBorderMode"), int(GB_BORDER_SGB));
        session->reset(EmulatedModel::Auto);
        QCOMPARE(Settings::instance().intValue(QStringLiteral("GBEmulatedModel")), int(EmulatedModel::Auto));
    }

    void saveStateRoundTrip()
    {
        auto session = openSession();
        QVERIFY(session);
        waitForGame(session.get());
        session->stop();
        QVERIFY(session->saveState(1));
        QVERIFY(QFile::exists(session->saveStatePath(1)));
        const uint8_t saved = readByte(session.get(), 0xC000);
        session->start();
        QTRY_VERIFY(readByte(session.get(), 0xC000) != saved);
        session->stop();
        session->loadState(1);
        QCOMPARE(readByte(session.get(), 0xC000), saved);
    }

    void legacyStateFallback()
    {
        auto session = openSession(QStringLiteral("legacy.gb"));
        QVERIFY(session);
        waitForGame(session.get());
        session->stop();
        QVERIFY(session->saveState(2));
        const uint8_t saved = readByte(session.get(), 0xC000);
        const QString modern = session->saveStatePath(2);
        const QString legacy = QString(modern).replace(QStringLiteral(".s2"), QStringLiteral(".sn2"));
        QVERIFY(QFile::rename(modern, legacy));
        session->start();
        QTRY_VERIFY(readByte(session.get(), 0xC000) != saved);
        session->stop();
        session->loadState(2);
        QCOMPARE(readByte(session.get(), 0xC000), saved);
    }

    void batterySave()
    {
        QString savPath;
        {
            auto session = openSession(QStringLiteral("battery.gb"));
            QVERIFY(session);
            waitForGame(session.get());
            savPath = session->savPath();
        } // Closing the session writes the battery
        QFile sav(savPath);
        QVERIFY(sav.open(QIODevice::ReadOnly));
        QCOMPARE(uint8_t(sav.read(1).at(0)), uint8_t(0x42));
    }

    void keyboardInput()
    {
        auto session = openSession();
        QVERIFY(session);
        InputController input(session.get());
        waitForGame(session.get());
        QCOMPARE(readByte(session.get(), 0xC001) & 1, 1); // A released (active low)
        QKeyEvent press(QEvent::KeyPress, Qt::Key_X, Qt::NoModifier);
        QVERIFY(input.keyEvent(&press, true));
        QTRY_COMPARE(readByte(session.get(), 0xC001) & 1, 0);
        QKeyEvent release(QEvent::KeyRelease, Qt::Key_X, Qt::NoModifier);
        QVERIFY(input.keyEvent(&release, false));
        QTRY_COMPARE(readByte(session.get(), 0xC001) & 1, 1);
        QKeyEvent unbound(QEvent::KeyPress, Qt::Key_F12, Qt::NoModifier);
        QVERIFY(!input.keyEvent(&unbound, true));
    }

    void rapidFire()
    {
        auto session = openSession();
        QVERIFY(session);
        Settings::instance().setValue(buttonPreferenceName(GBButton::RapidA, 0), int(Qt::Key_Q));
        InputController input(session.get());
        waitForGame(session.get());
        QKeyEvent press(QEvent::KeyPress, Qt::Key_Q, Qt::NoModifier);
        QVERIFY(input.keyEvent(&press, true));
        bool sawPressed = false, sawReleased = false;
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 2000 && !(sawPressed && sawReleased)) {
            const bool pressed = !(readByte(session.get(), 0xC001) & 1);
            sawPressed |= pressed;
            sawReleased |= !pressed;
            QTest::qWait(5);
        }
        QVERIFY(sawPressed);
        QVERIFY(sawReleased);
        QKeyEvent release(QEvent::KeyRelease, Qt::Key_Q, Qt::NoModifier);
        input.keyEvent(&release, false);
        Settings::instance().remove(buttonPreferenceName(GBButton::RapidA, 0));
    }

    void audioRecording()
    {
        auto session = openSession();
        QVERIFY(session);
        waitForGame(session.get());
        const QString path = m_directory.filePath(QStringLiteral("recording.wav"));
        QCOMPARE(session->startAudioRecording(path, GB_AUDIO_FORMAT_WAV), 0);
        QVERIFY(session->isRecordingAudio());
        QTest::qWait(300);
        QCOMPARE(session->stopAudioRecording(), 0);
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray header = file.read(12);
        QCOMPARE(header.left(4), QByteArray("RIFF"));
        QCOMPARE(header.mid(8, 4), QByteArray("WAVE"));
        QVERIFY(file.size() > 44 + 96000); // At least ~0.25 s of 16-bit stereo 96 kHz
    }

    void cheats()
    {
        auto session = openSession();
        QVERIFY(session);
        GB_gameboy_t *gb = session->gb();
        size_t count = 0;
        GB_get_cheats(gb, &count);
        const size_t before = count;
        bool valid = false, invalid = true;
        session->performAtomic([&] {
            valid = GB_import_cheat(gb, "01FF00C0", "Test", true) != nullptr;
            invalid = GB_import_cheat(gb, "not a code", "Bad", true) != nullptr;
        });
        QVERIFY(valid);
        QVERIFY(!invalid);
        GB_get_cheats(gb, &count);
        QCOMPARE(count, before + 1);
    }

    void memoryModel()
    {
        auto session = openSession();
        QVERIFY(session);
        waitForGame(session.get());
        MemoryModel model(session.get());
        model.setMode(MemoryModel::RAM);
        QCOMPARE(model.base(), uint16_t(0xC000));
        QCOMPARE(model.length(), size_t(0x2000));
        const uint8_t value = 0x99;
        model.write(0x10, &value, 1);
        QCOMPARE(readByte(session.get(), 0xC010), value);
        uint8_t readBack = 0;
        model.read(0x10, 1, &readBack);
        QCOMPARE(readBack, value);

        // ROM edits mark the session modified
        model.setMode(MemoryModel::ROM);
        QSignalSpy modified(session.get(), &EmulatorSession::romModifiedChanged);
        const uint8_t patch = 0xAA;
        model.write(0x7FFF, &patch, 1);
        QVERIFY(session->isROMModified());
        QCOMPARE(modified.count(), 1);
    }

    void linkCable()
    {
        auto first = openSession(QStringLiteral("link1.gb"));
        auto second = openSession(QStringLiteral("link2.gb"));
        QVERIFY(first && second);
        waitForGame(first.get());
        first->connectLinkCable(second.get());
        QCOMPARE(first->partner(), second.get());
        QCOMPARE(second->partner(), first.get());
        QVERIFY(second->isSlave());
        const uint8_t a = readByte(first.get(), 0xC000);
        const uint8_t b = readByte(second.get(), 0xC000);
        QTRY_VERIFY(readByte(first.get(), 0xC000) != a);
        QTRY_VERIFY(readByte(second.get(), 0xC000) != b);
        second.reset(); // Closing a linked session must leave the other running
        QVERIFY(!first->partner());
        const uint8_t c = readByte(first.get(), 0xC000);
        QTRY_VERIFY(readByte(first.get(), 0xC000) != c);
    }

    void accessories()
    {
        auto session = openSession();
        QVERIFY(session);
        session->connectPrinter();
        QCOMPARE(GB_get_built_in_accessory(session->gb()), GB_ACCESSORY_PRINTER);
        session->connectWorkboy();
        QCOMPARE(GB_get_built_in_accessory(session->gb()), GB_ACCESSORY_WORKBOY);
        session->disconnectAllAccessories();
        QCOMPARE(GB_get_built_in_accessory(session->gb()), GB_ACCESSORY_NONE);
    }

    void debuggerCommand()
    {
        auto session = openSession();
        QVERIFY(session);
        waitForGame(session.get());
        QString output;
        connect(session.get(), &EmulatorSession::consoleOutput, this, [&](const QList<LogChunk> &chunks) {
            for (const LogChunk &chunk : chunks) {
                output += chunk.text;
            }
        });
        session->breakDebugger();
        QTRY_VERIFY(session->isDebuggerStopped());
        session->queueDebuggerCommand(QStringLiteral("registers"));
        QTRY_VERIFY(output.contains(QStringLiteral("PC  =")));
        session->queueDebuggerCommand(QStringLiteral("continue"));
        QTRY_VERIFY(!session->isDebuggerStopped());
    }

    void evaluateExpression()
    {
        auto session = openSession();
        QVERIFY(session);
        uint16_t result = 0, bank = 0;
        const QString error =
            session->captureOutput([&] { QVERIFY(!GB_debugger_evaluate(session->gb(), "$10 + 5", &result, &bank)); });
        QCOMPARE(result, uint16_t(0x15));
        QVERIFY(error.isEmpty());
    }
};

QTEST_MAIN(SameBoyTests)
#include "tst_sameboy.moc"
