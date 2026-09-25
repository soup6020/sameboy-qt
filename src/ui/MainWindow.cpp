#include "MainWindow.h"
#include "AppController.h"
#include "CheatSearchWindow.h"
#include "CheatsWindow.h"
#include "DebuggerConsole.h"
#include "GbsPlayerWidget.h"
#include "MemoryViewer.h"
#include "OSDOverlay.h"
#include "PrinterWindow.h"
#include "ScreenWidget.h"
#include "VramViewer.h"
#include "core/CameraProvider.h"
#include "core/EmulatorSession.h"
#include "input/InputController.h"
#include "settings/Settings.h"

#include <QActionGroup>

#include <algorithm>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLocale>
#include <QMenuBar>
#include <QMessageBox>
#include <QScreen>
#include <QToolTip>
#include <QUrl>
#include <QWindow>

namespace {

MainWindow *g_lastActiveWindow = nullptr;
QList<MainWindow *> g_windows;

enum ResizeAction { Zoom, Increase, Decrease };

#ifdef Q_OS_MACOS
// On macOS Qt maps Ctrl to ⌘ and Meta to ⌃, matching the Cocoa shortcuts.
constexpr auto kBreakShortcut = Qt::META | Qt::Key_C;
constexpr auto kSaveROMShortcut = Qt::META | Qt::CTRL | Qt::Key_S;
#else
constexpr auto kBreakShortcut = Qt::CTRL | Qt::Key_C;
constexpr auto kSaveROMShortcut = Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_S;
#endif

} // namespace

MainWindow *MainWindow::lastActiveWindow()
{
    return g_lastActiveWindow;
}

QList<MainWindow *> MainWindow::allWindows()
{
    return g_windows;
}

MainWindow::MainWindow(const QString &path, QWidget *parent)
    : QMainWindow(parent), m_session(new EmulatorSession(path, this))
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowIcon(QIcon(QStringLiteral(":/icon.png")));
    g_windows.append(this);

    m_screen = new ScreenWidget(this);
    m_screen->setSession(m_session);
    m_screen->installEventFilter(this);
    setCentralWidget(m_screen);

    m_input = new InputController(m_session, this);
    m_input->setActivePredicate([this] {
        if (isActiveWindow()) {
            return true;
        }
        return Settings::instance().boolValue(QStringLiteral("GBAllowBackgroundControllers")) &&
               g_lastActiveWindow == this;
    });
    connect(m_input, &InputController::hotkey, this, &MainWindow::performHotkeyAction);
    connect(m_input, &InputController::controllerUsed, this, [this] {
        if (m_screen) {
            m_screen->setMouseControlEnabled(false);
        }
    });

    m_camera = new CameraProvider(m_session, this);

    connect(m_screen, &ScreenWidget::stateFileDropped, this, [this](const QString &file) {
        m_session->loadStateFile(file, false);
    });
    connect(m_screen, &ScreenWidget::romFilesDropped, &AppController::instance(), &AppController::openFiles);

    connect(m_session, &EmulatorSession::screenSizeChanged, this, &MainWindow::updateMinimumSize);
    connect(m_session, &EmulatorSession::runningChanged, this, &MainWindow::updateMouseHiding);
    connect(m_session, &EmulatorSession::warning, this, &MainWindow::showWarning);
    connect(m_session, &EmulatorSession::errorMessage, this, [this](const QString &message) {
        QMessageBox::critical(this, windowTitle(), message);
    });
    connect(m_session, &EmulatorSession::romModifiedChanged, this, [this](bool modified) {
        setWindowModified(modified);
    });
    connect(m_session, &EmulatorSession::gbsLoaded, this, &MainWindow::prepareGBSInterface);
    connect(m_session, &EmulatorSession::consoleOutput, this, [this](const QList<LogChunk> &chunks, bool clearSide) {
        if (!m_console && !Settings::instance().boolValue(QStringLiteral("DeveloperMode"))) {
            // Keep a console around anyway so output isn't lost; it's cheap.
        }
        DebuggerConsole *console = this->console();
        console->appendOutput(chunks, clearSide);
        if (!chunks.isEmpty() && Settings::instance().boolValue(QStringLiteral("DeveloperMode"))) {
            console->show();
            console->raise();
            m_screen->setMouseHidingEnabled(false);
        }
    });
    connect(m_session, &EmulatorSession::printerImage, this, [this](const QImage &image) {
        if (!m_printerWindow) {
            m_printerWindow = new PrinterWindow(m_session);
            m_printerWindow->setWindowTitle(tr("Printer – %1").arg(m_session->displayName()));
        }
        m_printerWindow->appendImage(image); // Starts a new feed if the window was hidden
        showPrinterWindow();
    });
    connect(m_session, &EmulatorSession::printerDone, this, [this] {
        if (m_printerWindow) {
            m_printerWindow->printingDone();
        }
    });
    connect(m_session, &EmulatorSession::alarmScheduled, &AppController::instance(),
            [this](unsigned seconds, const QString &name) {
                AppController::instance().scheduleAlarm(m_session->filePath(), name, seconds);
            });
    connect(m_session, &EmulatorSession::alarmCancelled, &AppController::instance(), &AppController::cancelAlarm);

    buildMenus();
    updateTitle();

    // Restore the last window size (Document.m LastWindowWidth/Height)
    Settings &settings = Settings::instance();
    const QSize lastSize(settings.intValue(QStringLiteral("LastWindowWidth")),
                         settings.intValue(QStringLiteral("LastWindowHeight")));
    resize(lastSize.expandedTo(QSize(160 * 3, 144 * 3 + menuBar()->sizeHint().height())));
}

MainWindow::~MainWindow()
{
    g_windows.removeAll(this);
    if (g_lastActiveWindow == this) {
        g_lastActiveWindow = nullptr;
    }
    for (QWidget *window : std::initializer_list<QWidget *>{m_console, m_memoryViewer, m_vramViewer, m_printerWindow,
                                                            m_cheatsWindow, m_cheatSearchWindow}) {
        delete window;
    }
    delete m_input;
    m_input = nullptr;
    // The session stops its thread and saves in its destructor.
    delete m_session;
    m_session = nullptr;
}

bool MainWindow::open()
{
    QString error;
    if (!m_session->open(&error)) {
        QMessageBox::critical(nullptr, tr("SameBoy"), error);
        return false;
    }
    updateMinimumSize();
    AppController::instance().noteRecentFile(m_session->filePath());
    return true;
}

QString MainWindow::filePath() const
{
    return m_session->filePath();
}

void MainWindow::updateTitle()
{
    setWindowTitle(QStringLiteral("%1[*]").arg(QFileInfo(m_session->filePath()).fileName()));
    setWindowFilePath(m_session->filePath());
}

void MainWindow::updateMinimumSize()
{
    if (m_gbsPlayer) {
        return;
    }
    const QSize screenSize(int(m_session->screenWidth()), int(m_session->screenHeight()));
    const QSize chrome = size() - m_screen->size();
    m_screen->setMinimumSize(screenSize);
    if (m_screen->width() < screenSize.width() || m_screen->height() < screenSize.height()) {
        resize(screenSize * 2 + chrome);
    }
}

void MainWindow::prepareGBSInterface()
{
    if (m_gbsPlayer) {
        m_gbsPlayer->refresh();
        return;
    }
    m_gbsPlayer = new GbsPlayerWidget(m_session, this);
    m_screen->removeEventFilter(this);
    m_screen->hide();
    setCentralWidget(m_gbsPlayer); // Deletes the screen widget
    m_screen = nullptr;
    m_gbsPlayer->installEventFilter(this);
    adjustSize();
    setFixedSize(sizeHint());
    m_increaseSizeAction->setEnabled(false);
    m_decreaseSizeAction->setEnabled(false);
}

// MARK: - Menus

void MainWindow::buildMenus()
{
    AppController &app = AppController::instance();
    QMenuBar *bar = menuBar();

    // File
    QMenu *file = bar->addMenu(tr("&File"));
    file->addAction(tr("&Open…"), QKeySequence::Open, &app, &AppController::showOpenDialog);
    file->addAction(tr("Hot Swap Cartridge…"), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_O), this, &MainWindow::cartSwap);
    file->addMenu(app.recentFilesMenu(file));
    file->addSeparator();
    file->addAction(tr("New Cartridge Instance…"), QKeySequence::New, this, &MainWindow::newCartridgeInstance);
    file->addSeparator();
    file->addAction(tr("&Close"), QKeySequence::Close, this, &QWidget::close);
    m_saveROMAction = file->addAction(tr("Save ROM Modifications"), QKeySequence(kSaveROMShortcut), this,
                                      [this] { saveROMModifications(false); });
    m_saveROMAsAction = file->addAction(tr("Save ROM Modifications As…"), this, [this] { saveROMModifications(true); });
    m_reloadAction = file->addAction(tr("Reload ROM"), this, &MainWindow::reloadROM);
    file->addSeparator();
    QAction *quit = file->addAction(tr("&Quit"), QKeySequence::Quit, &app, &AppController::quit);
    quit->setMenuRole(QAction::QuitRole);

    // Edit
    QMenu *edit = bar->addMenu(tr("&Edit"));
    QAction *preferences = edit->addAction(tr("Preferences…"), QKeySequence(Qt::CTRL | Qt::Key_Comma), &app,
                                           &AppController::showPreferences);
    preferences->setMenuRole(QAction::PreferencesRole);

    // Emulation
    QMenu *emulation = bar->addMenu(tr("E&mulation"));
    emulation->addAction(tr("Reset"), QKeySequence(Qt::CTRL | Qt::Key_R), this, [this] { reset(EmulatedModel::None); });
    emulation->addAction(tr("Quick Reset"), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_R), this,
                         [this] { reset(EmulatedModel::QuickReset); });
    m_reloadEmulationAction = emulation->addAction(tr("Reload ROM"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R), this,
                                                   &MainWindow::reloadROM);
    m_pauseAction = emulation->addAction(tr("Pause"), QKeySequence(Qt::CTRL | Qt::Key_P), this, &MainWindow::togglePause);
    m_pauseAction->setCheckable(true);

    QMenu *models = emulation->addMenu(tr("Emulated Model"));
    m_modelGroup = new QActionGroup(this);
    const std::pair<QString, EmulatedModel> modelItems[] = {
        {tr("Pick Automatically"), EmulatedModel::Auto},
        {QString(), EmulatedModel::None},
        {tr("Game Boy"), EmulatedModel::DMG},
        {tr("Game Boy Pocket/Light"), EmulatedModel::MGB},
        {tr("Super Game Boy"), EmulatedModel::SGB},
        {tr("Game Boy Color"), EmulatedModel::CGB},
        {tr("Game Boy Advance"), EmulatedModel::AGB},
    };
    for (const auto &[title, model] : modelItems) {
        if (model == EmulatedModel::None) {
            models->addSeparator();
            continue;
        }
        const EmulatedModel selected = model;
        QAction *action = models->addAction(title, this, [this, selected] { reset(selected); });
        action->setCheckable(true);
        action->setData(int(model));
        m_modelGroup->addAction(action);
        m_modelActions << action;
    }
    emulation->addSeparator();

    QMenu *saveStates = emulation->addMenu(tr("Save State"));
    QMenu *loadStates = emulation->addMenu(tr("Load State"));
    for (unsigned slot = 1; slot <= 10; slot++) {
        const Qt::Key key = Qt::Key(Qt::Key_0 + slot % 10);
        saveStates->addAction(tr("Slot %1").arg(slot), QKeySequence(Qt::CTRL | key), this, [this, slot] { saveState(slot); });
        loadStates->addAction(tr("Slot %1").arg(slot), QKeySequence(Qt::CTRL | Qt::SHIFT | key), this,
                              [this, slot] { loadState(slot); });
    }
    emulation->addSeparator();
    emulation->addAction(tr("Save Screenshot"), QKeySequence(Qt::CTRL | Qt::Key_S), this, &MainWindow::saveScreenshot);
    emulation->addAction(tr("Save Screenshot As…"), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_S), this,
                         &MainWindow::saveScreenshotAs);
    emulation->addAction(tr("Copy Screenshot"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S), this,
                         &MainWindow::copyScreenshot);
    emulation->addSeparator();
    m_audioRecordingAction = emulation->addAction(tr("Start Audio Recording…"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A),
                                                  this, &MainWindow::toggleAudioRecording);
    m_muteAction = emulation->addAction(tr("Mute Sound"), QKeySequence(Qt::CTRL | Qt::Key_M), this, &MainWindow::toggleMute);
    m_muteAction->setCheckable(true);

    // Cheats
    QMenu *cheats = bar->addMenu(tr("&Cheats"));
    m_cheatsEnabledAction = cheats->addAction(tr("Enable Cheats"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C), this, [this] {
        GB_set_cheats_enabled(m_session->gb(), !GB_cheats_enabled(m_session->gb()));
    });
    m_cheatsEnabledAction->setCheckable(true);
    cheats->addAction(tr("Show Cheats"), this, &MainWindow::showCheats);
    cheats->addAction(tr("Search Cheats"), this, &MainWindow::showCheatSearch);

    // Connect
    QMenu *connectMenu = bar->addMenu(tr("C&onnect"));
    m_disconnectAction = connectMenu->addAction(tr("None"), this, [this] { m_session->disconnectAllAccessories(); });
    m_disconnectAction->setCheckable(true);
    m_linkMenu = connectMenu->addMenu(tr("Game Link Cable && Infrared"));
    connect(m_linkMenu, &QMenu::aboutToShow, this, &MainWindow::populateLinkMenu);
    m_printerAction = connectMenu->addAction(tr("Game Boy Printer"), this, [this] { m_session->connectPrinter(); });
    m_printerAction->setCheckable(true);
    m_workboyAction = connectMenu->addAction(tr("Workboy"), this, [this] { m_session->connectWorkboy(); });
    m_workboyAction->setCheckable(true);

    // Develop
    QMenu *develop = bar->addMenu(tr("&Develop"));
    m_developerModeAction = develop->addAction(tr("Developer Mode"), this, [] {
        Settings &settings = Settings::instance();
        settings.setValue(QStringLiteral("DeveloperMode"), !settings.boolValue(QStringLiteral("DeveloperMode")));
    });
    m_developerModeAction->setCheckable(true);
    develop->addSeparator();
    develop->addAction(tr("Show Console"), this, &MainWindow::showConsole);
    develop->addAction(tr("Clear Console"), QKeySequence(Qt::CTRL | Qt::Key_K), this, [this] { console()->clear(); });
    develop->addSeparator();
    m_breakAction = develop->addAction(tr("Break Debugger"), QKeySequence(kBreakShortcut), this, &MainWindow::interrupt);
    develop->addSeparator();
    QMenu *channels = develop->addMenu(tr("Audio Channels"));
    const QString channelNames[] = {tr("Square Channel 1"), tr("Square Channel 2"), tr("Wave Channel"), tr("Noise Channel")};
    for (int i = 0; i < 4; i++) {
        m_channelActions[i] = channels->addAction(channelNames[i], QKeySequence(Qt::ALT | Qt::Key(Qt::Key_1 + i)), this, [this, i] {
            GB_set_channel_muted(m_session->gb(), GB_channel_t(i), !GB_is_channel_muted(m_session->gb(), GB_channel_t(i)));
        });
        m_channelActions[i]->setCheckable(true);
    }
    develop->addSeparator();
    m_showBackgroundAction = develop->addAction(tr("Show Background and Window"), this, [this] {
        GB_set_background_rendering_disabled(m_session->gb(), !GB_is_background_rendering_disabled(m_session->gb()));
    });
    m_showBackgroundAction->setCheckable(true);
    m_showObjectsAction = develop->addAction(tr("Show Objects"), this, [this] {
        GB_set_object_rendering_disabled(m_session->gb(), !GB_is_object_rendering_disabled(m_session->gb()));
    });
    m_showObjectsAction->setCheckable(true);
    develop->addSeparator();
    develop->addAction(tr("Show Memory"), this, &MainWindow::showMemory);
    develop->addAction(tr("Show VRAM Viewer"), this, &MainWindow::showVRAMViewer);

    // Window
    QMenu *window = bar->addMenu(tr("&Window"));
    window->addAction(tr("Minimize"), this, &QWidget::showMinimized);
    m_increaseSizeAction = window->addAction(tr("Increase Window Size"), QKeySequence(Qt::CTRL | Qt::Key_Plus), this,
                                             &MainWindow::increaseWindowSize);
    m_increaseSizeAction->setShortcuts({QKeySequence(Qt::CTRL | Qt::Key_Plus), QKeySequence(Qt::CTRL | Qt::Key_Equal)});
    m_decreaseSizeAction = window->addAction(tr("Decrease Window Size"), QKeySequence(Qt::CTRL | Qt::Key_Minus), this,
                                             &MainWindow::decreaseWindowSize);
    window->addAction(tr("Zoom"), this, &MainWindow::zoom);
    m_fullScreenAction = window->addAction(tr("Enter Full Screen"), QKeySequence::FullScreen, this, &MainWindow::toggleFullScreen);
    window->addSeparator();
    window->addAction(tr("Bring All to Front"), &app, &AppController::bringAllToFront);

    // Help
    QMenu *help = bar->addMenu(tr("&Help"));
    help->addAction(tr("Debugger Help"), [] {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://sameboy.github.io/debugger/")));
    });
    help->addSeparator();
    QAction *about = help->addAction(tr("About SameBoy"), &app, &AppController::showAbout);
    about->setMenuRole(QAction::AboutRole);
    help->addAction(tr("Sponsor SameBoy"), [] {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/sponsors/LIJI32")));
    });

    for (QMenu *menu : {file, emulation, cheats, connectMenu, develop, window}) {
        connect(menu, &QMenu::aboutToShow, this, &MainWindow::updateMenuStates);
    }
    // Shortcuts must stay usable when the menu bar is hidden in full screen.
    for (QAction *action : bar->findChildren<QAction *>()) {
        if (!action->menu() && !action->shortcut().isEmpty()) {
            addAction(action);
        }
    }
    updateMenuStates();
}

void MainWindow::updateMenuStates()
{
    // Port of -[Document validateUserInterfaceItem:]
    GB_gameboy_t *gb = m_session->gb();
    const bool inited = GB_is_inited(gb);
    m_muteAction->setChecked(m_session->isMuted());
    m_pauseAction->setChecked(m_session->isPaused());
    m_pauseAction->setEnabled(!(inited && GB_debugger_is_stopped(gb)));
    for (QAction *action : m_modelActions) {
        const auto model = EmulatedModel(action->data().toInt());
        action->setChecked((model == m_session->currentModel() && !m_session->usesAutoModel()) ||
                           (model == EmulatedModel::Auto && m_session->usesAutoModel()));
    }
    const bool developerMode = Settings::instance().boolValue(QStringLiteral("DeveloperMode"));
    m_developerModeAction->setChecked(developerMode);
    m_breakAction->setEnabled(developerMode);
    if (inited) {
        const GB_accessory_t accessory = GB_get_built_in_accessory(gb);
        m_disconnectAction->setChecked(accessory == GB_ACCESSORY_NONE && !m_session->partner());
        m_printerAction->setChecked(accessory == GB_ACCESSORY_PRINTER);
        m_workboyAction->setChecked(accessory == GB_ACCESSORY_WORKBOY);
        m_cheatsEnabledAction->setChecked(GB_cheats_enabled(gb));
        m_showBackgroundAction->setChecked(!GB_is_background_rendering_disabled(gb));
        m_showObjectsAction->setChecked(!GB_is_object_rendering_disabled(gb));
        for (int i = 0; i < 4; i++) {
            m_channelActions[i]->setChecked(!GB_is_channel_muted(gb, GB_channel_t(i)));
        }
    }
    m_linkMenu->setEnabled(EmulatorSession::allSessions().size() > 1);
    m_audioRecordingAction->setText(m_session->isRecordingAudio() ? tr("Stop Audio Recording") : tr("Start Audio Recording…"));
    m_increaseSizeAction->setEnabled(!m_gbsPlayer && newWindowSize(Increase, nullptr));
    m_decreaseSizeAction->setEnabled(!m_gbsPlayer && newWindowSize(Decrease, nullptr));
    m_reloadAction->setEnabled(!m_session->isGBS());
    m_reloadEmulationAction->setEnabled(!m_session->isGBS());
    m_saveROMAction->setEnabled(m_session->isROMModified());
    m_saveROMAsAction->setEnabled(m_session->isROMModified() && !m_session->isCartContainer());
    m_fullScreenAction->setText(isFullScreen() ? tr("Exit Full Screen") : tr("Enter Full Screen"));
}

void MainWindow::populateLinkMenu()
{
    // -[GBApp menuNeedsUpdate:]
    m_linkMenu->clear();
    for (EmulatorSession *other : EmulatorSession::allSessions()) {
        if (other == m_session) {
            continue;
        }
        QAction *action = m_linkMenu->addAction(other->displayName(), this, [this, other] {
            if (EmulatorSession::allSessions().contains(other)) {
                m_session->connectLinkCable(other);
            }
        });
        action->setCheckable(true);
        action->setChecked(m_session->partner() == other);
    }
}

// MARK: - Window events

void MainWindow::closeEvent(QCloseEvent *event)
{
    // NSDocument asks before discarding ROM modifications made in the memory viewer.
    if (m_session->isROMModified()) {
        const auto answer = QMessageBox::warning(
            this, tr("SameBoy"), tr("Do you want to save the changes made to the ROM “%1”?").arg(m_session->displayName()),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
        if (answer == QMessageBox::Cancel) {
            event->ignore();
            return;
        }
        if (answer == QMessageBox::Save && !m_session->writeROM(QString())) {
            QMessageBox::critical(this, tr("SameBoy"), tr("Could not write the modified ROM."));
            event->ignore();
            return;
        }
    }
    if (!m_gbsPlayer && !isFullScreen() && isVisible()) {
        Settings::instance().setValue(QStringLiteral("LastWindowWidth"), width());
        Settings::instance().setValue(QStringLiteral("LastWindowHeight"), height());
    }
    m_session->disconnectLinkCable();
    QMainWindow::closeEvent(event);
    // Closing the last game returns to the idle window instead of quitting.
    if (event->isAccepted() && !AppController::instance().isQuitting() &&
        std::none_of(g_windows.cbegin(), g_windows.cend(), [this](MainWindow *window) {
            return window != this && window->isVisible();
        })) {
        AppController::instance().showWelcome();
    }
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::ActivationChange) {
        if (isActiveWindow()) {
            g_lastActiveWindow = this;
            // -[Document showWindows]: pick up ROMs rebuilt behind our back
            m_session->checkForFileChanges();
        }
        else if (m_input) {
            m_input->releaseAll();
        }
    }
    else if (event->type() == QEvent::WindowStateChange) {
        menuBar()->setVisible(!isFullScreen());
        updateMouseHiding();
    }
    QMainWindow::changeEvent(event);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (m_input && m_input->keyEvent(keyEvent, event->type() == QEvent::KeyPress)) {
            return true;
        }
        if (event->type() == QEvent::KeyPress && keyEvent->key() == Qt::Key_Escape && isFullScreen()) {
            toggleFullScreen();
            return true;
        }
    }
    else if (event->type() == QEvent::ShortcutOverride) {
        // Let bound keys (e.g. Tab for rewind) reach the emulator instead of
        // being consumed as focus navigation.
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->modifiers() == Qt::NoModifier || keyEvent->modifiers() == Qt::KeypadModifier) {
            event->accept();
            return false;
        }
    }
    else if (event->type() == QEvent::MouseButtonPress && m_input) {
        m_input->setMouseControlEnabled(true);
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::updateMouseHiding()
{
    if (!m_screen) {
        return;
    }
    bool toolWindowVisible = false;
    for (QWidget *window : std::initializer_list<QWidget *>{m_console, m_memoryViewer, m_vramViewer, m_printerWindow,
                                                            m_cheatsWindow, m_cheatSearchWindow}) {
        toolWindowVisible |= window && window->isVisible();
    }
    m_screen->setMouseHidingEnabled(isFullScreen() && m_session->isRunning() && !toolWindowVisible);
}

void MainWindow::showWarning(const QString &text)
{
    QWidget *anchor = m_screen ? static_cast<QWidget *>(m_screen) : this;
    QToolTip::showText(anchor->mapToGlobal(QPoint(anchor->width() / 2, anchor->height() / 3)), text, anchor, {}, 8000);
}

// MARK: - Emulation actions

void MainWindow::reset(EmulatedModel model)
{
    m_session->reset(model);
    updateMinimumSize();
    if (m_memoryViewer) {
        m_memoryViewer->sessionReset();
    }
}

void MainWindow::togglePause()
{
    m_session->togglePause();
}

void MainWindow::saveState(unsigned slot)
{
    m_session->saveState(slot);
}

void MainWindow::loadState(unsigned slot)
{
    m_session->loadState(slot);
}

void MainWindow::toggleMute()
{
    const bool muted = !m_session->isMuted();
    m_session->setMuted(muted);
    if (!muted && Settings::instance().doubleValue(QStringLiteral("GBVolume")) == 0) {
        showWarning(tr("Warning: Volume is set to to zero in the preferences panel"));
    }
}

void MainWindow::performHotkeyAction(const QString &action)
{
    // Stored in GBJoypadHotkey1/2; see PreferencesDialog for the list.
    if (action == QLatin1String("togglePause")) {
        togglePause();
    }
    else if (action == QLatin1String("reset")) {
        reset(EmulatedModel::None);
    }
    else if (action == QLatin1String("mute")) {
        toggleMute();
    }
    else if (action.startsWith(QLatin1String("saveState:"))) {
        saveState(action.section(QLatin1Char(':'), 1).toUInt());
    }
    else if (action.startsWith(QLatin1String("loadState:"))) {
        loadState(action.section(QLatin1Char(':'), 1).toUInt());
    }
}

void MainWindow::reloadROM()
{
    m_session->reloadROM();
}

void MainWindow::cartSwap()
{
    const bool wasRunning = m_session->isRunning();
    if (wasRunning) {
        m_session->stop();
    }
    const QString path = QFileDialog::getOpenFileName(this, tr("Hot Swap Cartridge"), QFileInfo(filePath()).path(),
                                                      AppController::romFileFilter());
    if (!path.isEmpty()) {
        QString error;
        if (!m_session->hotSwap(path, &error)) {
            QMessageBox::critical(this, tr("SameBoy"), error);
        }
        else {
            updateTitle();
            AppController::instance().noteRecentFile(path);
        }
    }
    if (wasRunning) {
        m_session->start();
    }
}

void MainWindow::newCartridgeInstance()
{
    const bool wasRunning = m_session->isRunning();
    m_session->stop();
    QString path = QFileDialog::getSaveFileName(this, tr("New Cartridge Instance"), QFileInfo(filePath()).path(),
                                                tr("Game Boy Cartridge (*.gbcart)"));
    if (!path.isEmpty()) {
        if (!path.endsWith(QLatin1String(".gbcart"), Qt::CaseInsensitive)) {
            path += QStringLiteral(".gbcart");
        }
        const QString romPath = m_session->romPath();
        QDir(path).removeRecursively();
        QDir().mkpath(path);
        QFile gbl(path + QStringLiteral("/rom.gbl"));
        if (gbl.open(QIODevice::WriteOnly)) {
            gbl.write(QStringLiteral("%1\n%2\n").arg(QDir(path).relativeFilePath(romPath), romPath).toUtf8());
            gbl.close();
            AppController::instance().openFiles({path});
        }
    }
    if (wasRunning) {
        m_session->start();
    }
}

void MainWindow::saveROMModifications(bool saveAs)
{
    QString target;
    if (saveAs) {
        target = QFileDialog::getSaveFileName(this, tr("Save ROM Modifications As"), filePath());
        if (target.isEmpty()) {
            return;
        }
    }
    if (!m_session->writeROM(target)) {
        QMessageBox::critical(this, tr("SameBoy"), tr("Could not write the modified ROM."));
    }
}

void MainWindow::interrupt()
{
    m_session->breakDebugger();
    DebuggerConsole *console = this->console();
    console->show();
    console->raise();
    console->activateWindow();
    console->focusInput();
}

// MARK: - Screenshots

QImage MainWindow::takeScreenshot()
{
    QImage image;
    if (m_screen && Settings::instance().boolValue(QStringLiteral("GBFilterScreenshots"))) {
        image = m_screen->renderToImage();
    }
    if (image.isNull()) {
        image = m_session->currentFrameImage();
    }
    return image;
}

QString MainWindow::screenshotFilename() const
{
    const QString date = QLocale().toString(QDateTime::currentDateTime(), QLocale::LongFormat);
    QString name = QStringLiteral("%1 – %2.png").arg(QFileInfo(filePath()).completeBaseName(), date);
    return name.replace(QLatin1Char(':'), QLatin1Char('.')).replace(QLatin1Char('/'), QLatin1Char('-'));
}

void MainWindow::saveScreenshot()
{
    Settings &settings = Settings::instance();
    QString folder = settings.stringValue(QStringLiteral("GBScreenshotFolder"));
    if (folder.isEmpty() || !QFileInfo(folder).isDir()) {
        const bool wasRunning = m_session->isRunning();
        m_session->stop();
        folder = QFileDialog::getExistingDirectory(this, tr("Choose a folder for screenshots"));
        if (wasRunning) {
            m_session->start();
        }
        if (folder.isEmpty()) {
            return;
        }
        settings.setValue(QStringLiteral("GBScreenshotFolder"), folder);
    }
    const QImage image = takeScreenshot();
    QString filename = QDir(folder).filePath(screenshotFilename());
    unsigned i = 2;
    while (QFileInfo::exists(filename)) {
        filename = QDir(folder).filePath(QFileInfo(screenshotFilename()).completeBaseName() + QStringLiteral(" %1.png").arg(i++));
    }
    if (image.save(filename, "PNG")) {
        m_session->showOSD(tr("Screenshot saved"));
    }
    else {
        QApplication::beep();
    }
}

void MainWindow::saveScreenshotAs()
{
    const bool wasRunning = m_session->isRunning();
    m_session->stop();
    const QImage image = takeScreenshot();
    Settings &settings = Settings::instance();
    const QString folder = settings.stringValue(QStringLiteral("GBScreenshotFolder"));
    const QString path = QFileDialog::getSaveFileName(this, tr("Save Screenshot"),
                                                      QDir(folder.isEmpty() ? QDir::homePath() : folder).filePath(screenshotFilename()),
                                                      tr("PNG Image (*.png)"));
    if (!path.isEmpty()) {
        if (image.save(path, "PNG")) {
            settings.setValue(QStringLiteral("GBScreenshotFolder"), QFileInfo(path).path());
            m_session->showOSD(tr("Screenshot saved"));
        }
        else {
            QApplication::beep();
        }
    }
    if (wasRunning) {
        m_session->start();
    }
}

void MainWindow::copyScreenshot()
{
    QApplication::clipboard()->setImage(takeScreenshot());
    m_session->showOSD(tr("Screenshot copied"));
}

// MARK: - Audio recording

void MainWindow::toggleAudioRecording()
{
    const bool wasRunning = m_session->isRunning();
    if (m_session->isRecordingAudio()) {
        const int error = m_session->stopAudioRecording();
        if (error) {
            QMessageBox::critical(this, tr("SameBoy"),
                                  tr("Could not finalize recording: %1").arg(QString::fromLocal8Bit(strerror(error))));
        }
        return;
    }
    m_session->stop();
    const QString aiff = tr("Apple AIFF (*.aiff *.aif *.aifc)");
    const QString wav = tr("RIFF WAVE (*.wav)");
    const QString raw = tr("Raw PCM, Stereo 96KHz 16-bit LE (*.raw *.pcm)");
    QString selectedFilter = wav;
    QString path = QFileDialog::getSaveFileName(this, tr("Start Audio Recording"), QFileInfo(filePath()).path(),
                                                QStringList{aiff, wav, raw}.join(QStringLiteral(";;")), &selectedFilter);
    if (!path.isEmpty()) {
        GB_audio_format_t format = GB_AUDIO_FORMAT_WAV;
        QString extension = QStringLiteral("wav");
        if (selectedFilter == aiff) {
            format = GB_AUDIO_FORMAT_AIFF;
            extension = QStringLiteral("aiff");
        }
        else if (selectedFilter == raw) {
            format = GB_AUDIO_FORMAT_RAW;
            extension = QStringLiteral("raw");
        }
        if (QFileInfo(path).suffix().isEmpty()) {
            path += QLatin1Char('.') + extension;
        }
        const int error = m_session->startAudioRecording(path, format);
        if (error) {
            QMessageBox::critical(this, tr("SameBoy"),
                                  tr("Could not start recording: %1").arg(QString::fromLocal8Bit(strerror(error))));
        }
    }
    if (wasRunning) {
        m_session->start();
    }
}

// MARK: - Window sizing (Document.m -newRect:forWindow:action:)

bool MainWindow::newWindowSize(int action, QSize *result) const
{
    if (isFullScreen() || !m_screen) {
        return false;
    }
    const int width = int(m_session->screenWidth());
    const int height = int(m_session->screenHeight());
    const double dpr = devicePixelRatioF();
    const double stepX = width / dpr;
    const double stepY = height / dpr;
    const QSize content = m_screen->size();
    const QSize chrome = size() - content;

    if (action == Decrease && (content.width() <= width || content.height() <= height)) {
        return false;
    }
    auto round = action == Decrease ? static_cast<double (*)(double)>(std::ceil) : static_cast<double (*)(double)>(std::floor);
    const double factor = std::min(round(content.width() / stepX), round(content.height() / stepY));
    double newWidth = factor * stepX;
    double newHeight = factor * stepY;
    if (action == Decrease) {
        newWidth -= stepX;
        newHeight -= stepY;
    }
    else {
        newWidth += stepX;
        newHeight += stepY;
    }
    const QRect available = screen() ? screen()->availableGeometry() : QRect(0, 0, 4096, 4096);
    if (newWidth + chrome.width() > available.width() || newHeight + chrome.height() > available.height()) {
        if (action == Increase) {
            return false;
        }
        newWidth = width;
        newHeight = height;
    }
    if (result) {
        *result = QSize(int(newWidth), int(newHeight)) + chrome;
    }
    return true;
}

void MainWindow::increaseWindowSize()
{
    QSize size;
    if (newWindowSize(Increase, &size)) {
        resize(size);
    }
}

void MainWindow::decreaseWindowSize()
{
    QSize size;
    if (newWindowSize(Decrease, &size)) {
        resize(size);
    }
}

void MainWindow::zoom()
{
    if (isMaximized()) {
        showNormal();
        return;
    }
    QSize size;
    if (newWindowSize(Zoom, &size)) {
        resize(size);
    }
}

void MainWindow::toggleFullScreen()
{
    if (isFullScreen()) {
        showNormal();
    }
    else {
        showFullScreen();
    }
}

// MARK: - Tool windows

DebuggerConsole *MainWindow::console()
{
    if (!m_console) {
        m_console = new DebuggerConsole(m_session);
        m_console->setWindowTitle(tr("Debug Console – %1").arg(m_session->displayName()));
        connect(m_console, &DebuggerConsole::visibilityChanged, this, &MainWindow::updateMouseHiding);
    }
    return m_console;
}

void MainWindow::showConsole()
{
    console()->show();
    console()->raise();
    console()->activateWindow();
}

void MainWindow::showMemory()
{
    if (!m_memoryViewer) {
        m_memoryViewer = new MemoryViewer(m_session);
        m_memoryViewer->setWindowTitle(tr("Memory – %1").arg(m_session->displayName()));
    }
    m_memoryViewer->show();
    m_memoryViewer->raise();
    m_memoryViewer->activateWindow();
    updateMouseHiding();
}

void MainWindow::showVRAMViewer()
{
    if (!m_vramViewer) {
        m_vramViewer = new VramViewer(m_session);
        m_vramViewer->setWindowTitle(tr("VRAM Viewer – %1").arg(m_session->displayName()));
    }
    m_vramViewer->show();
    m_vramViewer->raise();
    m_vramViewer->activateWindow();
    updateMouseHiding();
}

CheatsWindow *MainWindow::cheatsWindow()
{
    if (!m_cheatsWindow) {
        m_cheatsWindow = new CheatsWindow(m_session);
        m_cheatsWindow->setWindowTitle(tr("Cheats – %1").arg(m_session->displayName()));
    }
    return m_cheatsWindow;
}

void MainWindow::showCheats()
{
    cheatsWindow()->show();
    cheatsWindow()->raise();
    cheatsWindow()->activateWindow();
    updateMouseHiding();
}

void MainWindow::showCheatSearch()
{
    if (!m_cheatSearchWindow) {
        m_cheatSearchWindow = new CheatSearchWindow(m_session);
        m_cheatSearchWindow->setWindowTitle(tr("Cheat Search – %1").arg(m_session->displayName()));
        connect(m_cheatSearchWindow, &CheatSearchWindow::cheatAdded, this, [this](int row) {
            showCheats();
            cheatsWindow()->selectRow(row);
        });
    }
    m_cheatSearchWindow->show();
    m_cheatSearchWindow->raise();
    m_cheatSearchWindow->activateWindow();
    updateMouseHiding();
}

void MainWindow::showPrinterWindow()
{
    if (!m_printerWindow->isVisible()) {
        m_printerWindow->show();
    }
    updateMouseHiding();
}
