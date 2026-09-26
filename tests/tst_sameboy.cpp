// Automated checks for the emulation layer, run against a tiny RGBDS-built ROM
// (tests/testrom.asm). They exercise the same code paths the UI uses.

#include "core/EmulatorSession.h"
#include "input/InputController.h"
#include "settings/PaletteThemes.h"
#include "settings/Settings.h"
#include "ui/MemoryModel.h"

#include <QFile>
#include <QKeyEvent>
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
