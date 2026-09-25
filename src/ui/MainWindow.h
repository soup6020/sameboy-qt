#pragma once

#include "core/Models.h"

#include <QMainWindow>
#include <QPointer>

class EmulatorSession;
class InputController;
class ScreenWidget;
class GbsPlayerWidget;
class DebuggerConsole;
class MemoryViewer;
class VramViewer;
class PrinterWindow;
class CheatsWindow;
class CheatSearchWindow;
class CameraProvider;
class QAction;
class QActionGroup;
class QMenu;

// One window per emulation session; the Qt counterpart of a Cocoa Document's
// main window plus the MainMenu.xib actions that target it.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const QString &path, QWidget *parent = nullptr);
    ~MainWindow() override;

    // Loads the ROM; returns false (after showing an error) on failure.
    bool open();
    EmulatorSession *session() const { return m_session; }
    QString filePath() const;

    static MainWindow *lastActiveWindow();
    static QList<MainWindow *> allWindows();

    CheatsWindow *cheatsWindow();
    DebuggerConsole *console();

    void performHotkeyAction(const QString &action);

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void buildMenus();
    void updateMenuStates();
    void updateTitle();
    void updateMinimumSize();
    void updateMouseHiding();
    void prepareGBSInterface();

    // Actions (named after their Cocoa selectors)
    void reset(EmulatedModel model);
    void togglePause();
    void saveState(unsigned slot);
    void loadState(unsigned slot);
    void saveScreenshot();
    void saveScreenshotAs();
    void copyScreenshot();
    QImage takeScreenshot();
    QString screenshotFilename() const;
    void toggleAudioRecording();
    void toggleMute();
    void cartSwap();
    void reloadROM();
    void newCartridgeInstance();
    void saveROMModifications(bool saveAs);
    void increaseWindowSize();
    void decreaseWindowSize();
    void zoom();
    void toggleFullScreen();
    bool newWindowSize(int action, QSize *size) const;
    void interrupt();
    void showConsole();
    void showMemory();
    void showVRAMViewer();
    void showCheats();
    void showCheatSearch();
    void showPrinterWindow();
    void populateLinkMenu();
    void showWarning(const QString &text);

    EmulatorSession *m_session;
    InputController *m_input = nullptr;
    ScreenWidget *m_screen = nullptr;
    GbsPlayerWidget *m_gbsPlayer = nullptr;

    QPointer<DebuggerConsole> m_console;
    QPointer<MemoryViewer> m_memoryViewer;
    QPointer<VramViewer> m_vramViewer;
    QPointer<PrinterWindow> m_printerWindow;
    QPointer<CheatsWindow> m_cheatsWindow;
    QPointer<CheatSearchWindow> m_cheatSearchWindow;
    CameraProvider *m_camera = nullptr;

    // Menu items whose state is validated on show
    QAction *m_pauseAction = nullptr;
    QAction *m_muteAction = nullptr;
    QAction *m_audioRecordingAction = nullptr;
    QAction *m_reloadAction = nullptr;
    QAction *m_reloadEmulationAction = nullptr;
    QAction *m_saveROMAction = nullptr;
    QAction *m_saveROMAsAction = nullptr;
    QAction *m_cheatsEnabledAction = nullptr;
    QAction *m_disconnectAction = nullptr;
    QAction *m_printerAction = nullptr;
    QAction *m_workboyAction = nullptr;
    QMenu *m_linkMenu = nullptr;
    QAction *m_developerModeAction = nullptr;
    QAction *m_breakAction = nullptr;
    QAction *m_showBackgroundAction = nullptr;
    QAction *m_showObjectsAction = nullptr;
    QAction *m_channelActions[4] = {};
    QAction *m_increaseSizeAction = nullptr;
    QAction *m_decreaseSizeAction = nullptr;
    QAction *m_fullScreenAction = nullptr;
    QActionGroup *m_modelGroup = nullptr;
    QList<QAction *> m_modelActions;
    bool m_warningShown = false;
};
