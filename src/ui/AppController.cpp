#include "AppController.h"
#include "MainWindow.h"
#include "PreferencesDialog.h"
#include "WelcomeWindow.h"
#include "core/EmulatorSession.h"
#include "core/ResourceLocator.h"
#include "settings/Settings.h"

#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileOpenEvent>
#include <QMenu>
#include <QMessageBox>
#include <QSystemTrayIcon>
#include <QTimer>

AppController &AppController::instance()
{
    static AppController *controller = new AppController;
    return *controller;
}

AppController::AppController()
{
    qApp->installEventFilter(this);
}

QString AppController::romFileFilter()
{
    return tr("Game Boy Files (*.gb *.gbc *.sgb *.isx *.gbs *.gbcart rom.gbl);;All Files (*)");
}

bool AppController::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == qApp && event->type() == QEvent::FileOpen) {
        openFiles({static_cast<QFileOpenEvent *>(event)->file()});
        return true;
    }
    return QObject::eventFilter(watched, event);
}

void AppController::openFiles(const QStringList &paths)
{
    for (QString path : paths) {
        QFileInfo info(path);
        // A .gbcart is a directory; selecting its rom.gbl opens the container.
        if (info.fileName() == QLatin1String("rom.gbl") && info.dir().dirName().endsWith(QLatin1String(".gbcart"))) {
            path = info.path();
            info = QFileInfo(path);
        }
        // Focus an existing window for the same file, like NSDocumentController.
        bool found = false;
        for (MainWindow *window : MainWindow::allWindows()) {
            if (QFileInfo(window->filePath()) == info) {
                window->show();
                window->raise();
                window->activateWindow();
                found = true;
                break;
            }
        }
        if (found) {
            continue;
        }
        auto *window = new MainWindow(info.absoluteFilePath());
        if (!window->open()) {
            window->close();
            continue;
        }
        window->show();
        window->raise();
        window->activateWindow();
        // A game is open now; the idle window is no longer needed.
        if (m_welcome) {
            m_welcome->close();
        }
    }
}

void AppController::showWelcome()
{
    if (!m_welcome) {
        m_welcome = new WelcomeWindow;
    }
    m_welcome->show();
    m_welcome->raise();
    m_welcome->activateWindow();
}

void AppController::quit()
{
    m_quitting = true;
    QApplication::closeAllWindows();
    // Something (e.g. an unsaved-changes prompt) may have cancelled the quit.
    if (!MainWindow::allWindows().isEmpty()) {
        m_quitting = false;
        return;
    }
    QApplication::quit();
}

void AppController::showOpenDialog()
{
    QString directory;
    if (MainWindow *window = MainWindow::lastActiveWindow()) {
        directory = QFileInfo(window->filePath()).path();
    }
    else {
        const QStringList recent = Settings::instance().value(QStringLiteral("RecentFiles")).toStringList();
        if (!recent.isEmpty()) {
            directory = QFileInfo(recent.first()).path();
        }
    }
    const QStringList files = QFileDialog::getOpenFileNames(nullptr, tr("Open"), directory, romFileFilter());
    openFiles(files);
}

void AppController::showPreferences()
{
    if (!m_preferences) {
        m_preferences = new PreferencesDialog;
        m_preferences->setAttribute(Qt::WA_DeleteOnClose, false);
    }
    m_preferences->show();
    m_preferences->raise();
    m_preferences->activateWindow();
}

void AppController::showAbout()
{
    QMessageBox box;
    box.setWindowTitle(tr("About SameBoy"));
    box.setIconPixmap(QPixmap(QStringLiteral(":/icon.png")));
    box.setText(QStringLiteral("<h2>SameBoy</h2><p>Version %1 (Qt frontend)</p>").arg(QStringLiteral(GB_VERSION)));
    box.setInformativeText(tr("Copyright © 2015-%1 Lior Halphon<br>Qt frontend built on the unmodified SameBoy core.<br>"
                              "<a href=\"https://sameboy.github.io\">sameboy.github.io</a>")
                               .arg(QStringLiteral(GB_COPYRIGHT_YEAR)));
    box.setTextFormat(Qt::RichText);
    box.setDetailedText(ResourceLocator::licenseText());
    box.exec();
}

void AppController::bringAllToFront()
{
    for (QWidget *widget : QApplication::topLevelWidgets()) {
        if (widget->isVisible()) {
            widget->raise();
        }
    }
}

QMenu *AppController::recentFilesMenu(QWidget *parent)
{
    auto *menu = new QMenu(tr("Open Recent"), parent);
    connect(menu, &QMenu::aboutToShow, this, [this, menu] { populateRecentMenu(menu); });
    populateRecentMenu(menu);
    return menu;
}

void AppController::populateRecentMenu(QMenu *menu)
{
    menu->clear();
    const QStringList recent = Settings::instance().value(QStringLiteral("RecentFiles")).toStringList();
    for (const QString &path : recent) {
        menu->addAction(QFileInfo(path).fileName(), this, [this, path] { openFiles({path}); })->setToolTip(path);
    }
    menu->addSeparator();
    menu->addAction(tr("Clear Menu"), this, [] { Settings::instance().remove(QStringLiteral("RecentFiles")); })
        ->setEnabled(!recent.isEmpty());
}

void AppController::noteRecentFile(const QString &path)
{
    QStringList recent = Settings::instance().value(QStringLiteral("RecentFiles")).toStringList();
    recent.removeAll(path);
    recent.prepend(path);
    while (recent.size() > 10) {
        recent.removeLast();
    }
    Settings::instance().setValue(QStringLiteral("RecentFiles"), recent);
}

void AppController::scheduleAlarm(const QString &path, const QString &friendlyName, unsigned seconds)
{
    // Cocoa schedules an NSUserNotification; we can only notify while running.
    cancelAlarm(path);
    auto *timer = new QTimer(this);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, this, [this, path, friendlyName] {
        if (!m_tray) {
            m_tray = new QSystemTrayIcon(QIcon(QStringLiteral(":/icon.png")), this);
        }
        m_tray->show();
        m_tray->showMessage(tr("%1 Played an Alarm").arg(friendlyName),
                            tr("%1 requested your attention by playing a scheduled alarm").arg(friendlyName));
        cancelAlarm(path);
    });
    timer->start(int(qMin<qint64>(qint64(seconds) * 1000, std::numeric_limits<int>::max())));
    m_alarms.insert(path, timer);
}

void AppController::cancelAlarm(const QString &path)
{
    if (QTimer *timer = m_alarms.take(path)) {
        timer->deleteLater();
    }
}
