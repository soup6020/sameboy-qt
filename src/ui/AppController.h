#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>

class QMenu;
class QMenuBar;
class QSystemTrayIcon;
class QTimer;
class PreferencesDialog;
class WelcomeWindow;

// Application-wide actions (GBApp.m + NSDocumentController equivalents).
class AppController : public QObject
{
    Q_OBJECT

public:
    static AppController &instance();

    static QString romFileFilter();

    void openFiles(const QStringList &paths);
    void showOpenDialog();
    void showPreferences(); // Opens on the last-used tab
    void showPreferencesTab(int tab); // PreferencesDialog::Tab

    // The Settings menu shared by game windows and the idle window: one entry
    // per Preferences tab, plus Preferences… (moved to the app menu on macOS).
    QMenu *addSettingsMenu(QMenuBar *bar);
    void showAbout();
    void bringAllToFront();

    // The idle window shown while no game is open.
    void showWelcome();
    // Closes every window and exits (unlike closing the last game window,
    // which returns to the welcome window).
    void quit();
    bool isQuitting() const { return m_quitting; }

    QMenu *recentFilesMenu(QWidget *parent);
    void noteRecentFile(const QString &path);

    void scheduleAlarm(const QString &path, const QString &friendlyName, unsigned seconds);
    void cancelAlarm(const QString &path);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    AppController();
    void populateRecentMenu(QMenu *menu);

    QPointer<PreferencesDialog> m_preferences;
    QPointer<WelcomeWindow> m_welcome;
    bool m_quitting = false;
    QSystemTrayIcon *m_tray = nullptr;
    QHash<QString, QTimer *> m_alarms;
};
