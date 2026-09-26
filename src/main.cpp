#include "core/EmulatorSession.h"
#include "settings/Settings.h"
#include "ui/AppController.h"
#include "ui/MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QCommandLineParser>
#include <QMenu>
#include <QSurfaceFormat>
#include <QTabWidget>
#include <QTimer>
#include <QWidget>

#include <SDL3/SDL.h>

#include <cstdio>

// Headless mode used for automated checks: run a ROM for |milliseconds| and
// write the raw frame to |output|.
static int runHeadless(const QString &rom, const QString &output, int milliseconds)
{
    auto *session = new EmulatorSession(rom);
    QString error;
    if (!session->open(&error)) {
        fprintf(stderr, "%s\n", qPrintable(error));
        delete session;
        return 1;
    }
    int result = 0;
    QTimer::singleShot(milliseconds, qApp, [&] {
        if (!session->currentFrameImage().save(output, "PNG")) {
            fprintf(stderr, "Could not write %s\n", qPrintable(output));
            result = 1;
        }
        delete session;
        qApp->quit();
    });
    qApp->exec();
    return result;
}

int main(int argc, char **argv)
{
    QApplication::setApplicationName(QStringLiteral("sameboy-qt"));
    QApplication::setOrganizationName(QStringLiteral("sameboy-qt"));
    QApplication::setApplicationDisplayName(QStringLiteral("SameBoy"));
    QApplication::setApplicationVersion(QStringLiteral(GB_VERSION));
    QApplication::setDesktopFileName(QStringLiteral("sameboy-qt"));

    // upstream's GLSL shaders are "#version 150", i.e. OpenGL 3.2 core.
    // SAMEBOY_QT_SOFTWARE_RENDERER forces the QPainter path (compatibility context).
    QSurfaceFormat format;
    if (!qEnvironmentVariableIsSet("SAMEBOY_QT_SOFTWARE_RENDERER")) {
        format.setVersion(3, 2);
        format.setProfile(QSurfaceFormat::CoreProfile);
    }
    format.setSwapInterval(1);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(QStringLiteral(":/icon.png")));

    SDL_SetHint(SDL_HINT_APP_NAME, "SameBoy");
    if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        qWarning("sameboy-qt: SDL_Init failed: %s", SDL_GetError());
    }

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("SameBoy Game Boy emulator (Qt frontend)"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption screenshotOption(QStringLiteral("screenshot"),
                                        QStringLiteral("Headless: run the ROM, save the frame to <file>, and exit."),
                                        QStringLiteral("file"));
    QCommandLineOption durationOption(QStringLiteral("duration"),
                                      QStringLiteral("Milliseconds to run in --screenshot mode (default 5000)."),
                                      QStringLiteral("ms"), QStringLiteral("5000"));
    QCommandLineOption dumpSettingsOption(QStringLiteral("dump-settings"),
                                          QStringLiteral("Print all settings and exit."));
    parser.addOption(screenshotOption);
    parser.addOption(durationOption);
    parser.addOption(dumpSettingsOption);
    parser.addPositionalArgument(QStringLiteral("files"), QStringLiteral("ROM, GBS, ISX or .gbcart files to open."),
                                 QStringLiteral("[files...]"));
    parser.process(app);

    if (parser.isSet(dumpSettingsOption)) {
        Settings &settings = Settings::instance();
        for (const QString &key : settings.allKeys()) {
            const QVariant value = settings.value(key);
            const QString text = value.typeId() == QMetaType::QVariantMap
                ? QStringLiteral("{%1 entries}").arg(value.toMap().size())
                : value.toString();
            printf("%s = %s\n", qPrintable(key), qPrintable(text));
        }
        SDL_Quit();
        return 0;
    }

    const QStringList files = parser.positionalArguments();
    if (parser.isSet(screenshotOption)) {
        if (files.size() != 1) {
            fprintf(stderr, "--screenshot needs exactly one ROM\n");
            return 1;
        }
        const int result =
            runHeadless(files.first(), parser.value(screenshotOption), parser.value(durationOption).toInt());
        SDL_Quit();
        return result;
    }

    AppController &controller = AppController::instance();
    if (!files.isEmpty()) {
        controller.openFiles(files);
    }
    if (MainWindow::allWindows().isEmpty()) {
        // Start idle, showing the SDL frontend's logo, rather than with an open panel.
        controller.showWelcome();
    }
    // Developer hook: SAMEBOY_QT_VIRTUAL_GAMEPAD=1 attaches an SDL virtual gamepad,
    // so controller UI can be checked without hardware.
    if (qEnvironmentVariableIsSet("SAMEBOY_QT_VIRTUAL_GAMEPAD")) {
        SDL_VirtualJoystickDesc desc;
        SDL_INIT_INTERFACE(&desc);
        desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
        desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
        desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
        desc.name = "SameBoy Virtual Gamepad";
        SDL_AttachVirtualJoystick(&desc);
    }

    // Developer hook for automated UI checks: SAMEBOY_QT_TRIGGER="Show Console;Show Memory"
    // triggers menu items (by title, without mnemonics or ellipses) of the first window after 1 s.
    const QString triggers = qEnvironmentVariable("SAMEBOY_QT_TRIGGER");
    if (!triggers.isEmpty()) {
        QTimer::singleShot(1000, &app, [triggers] {
            MainWindow *window = MainWindow::allWindows().value(0);
            if (!window) {
                return;
            }
            const auto actions = window->findChildren<QAction *>();
            for (const QString &title : triggers.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
                for (QAction *action : actions) {
                    QString text = action->text();
                    text.remove(QLatin1Char('&')).remove(QStringLiteral("…"));
                    if (text == title && !action->menu()) {
                        action->trigger();
                        break;
                    }
                }
            }
        });
    }

    // Developer hook for automated UI checks: SAMEBOY_QT_GRAB=<prefix>:<ms>
    // grabs every visible top-level window to <prefix>-<n>.png and quits.
    const QString grab = qEnvironmentVariable("SAMEBOY_QT_GRAB");
    if (!grab.isEmpty()) {
        const QString prefix = grab.section(QLatin1Char(':'), 0, -2);
        const int delay = grab.section(QLatin1Char(':'), -1).toInt();
        QTimer::singleShot(delay, &app, [prefix] {
            int index = 0;
            for (QWidget *widget : QApplication::topLevelWidgets()) {
                if (!widget->isVisible()) {
                    continue;
                }
                const QString name =
                    QStringLiteral("%1-%2-%3").arg(prefix).arg(index++).arg(widget->metaObject()->className());
                widget->grab().save(name + QStringLiteral(".png"));
                for (QTabWidget *tabs : widget->findChildren<QTabWidget *>()) {
                    for (int tab = 1; tab < tabs->count(); tab++) {
                        tabs->setCurrentIndex(tab);
                        widget->grab().save(QStringLiteral("%1-tab%2.png").arg(name).arg(tab));
                    }
                }
            }
            QApplication::closeAllWindows();
            QApplication::quit();
        });
    }
    const int result = app.exec();
    SDL_Quit();
    return result;
}
