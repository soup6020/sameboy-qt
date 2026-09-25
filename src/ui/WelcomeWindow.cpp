#include "WelcomeWindow.h"
#include "AppController.h"
#include "core/ResourceLocator.h"
#include "settings/PaletteThemes.h"
#include "settings/Settings.h"

#include <QDesktopServices>
#include <QDragEnterEvent>
#include <QMenuBar>
#include <QMimeData>
#include <QPainter>
#include <QUrl>

WelcomeWindow::WelcomeWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(QStringLiteral("SameBoy"));
    setWindowIcon(QIcon(QStringLiteral(":/icon.png")));
    setAcceptDrops(true);
    // The menu bar sits above the central area; keep an (empty) central widget
    // so the logo is laid out below it.
    auto *central = new QWidget(this);
    central->setAttribute(Qt::WA_TransparentForMouseEvents);
    setCentralWidget(central);
    buildMenus();

    Settings &settings = Settings::instance();
    for (const char *key : {"GBColorPalette", "GBCurrentTheme", "GBThemes"}) {
        settings.observe(this, QString::fromLatin1(key), [this](const QVariant &) { updatePalette(); }, false);
    }
    updatePalette();

    const QSize lastSize(settings.intValue(QStringLiteral("LastWindowWidth")),
                         settings.intValue(QStringLiteral("LastWindowHeight")));
    setMinimumSize(160, 144 + menuBar()->sizeHint().height());
    resize(lastSize.expandedTo(QSize(160 * 3, 144 * 3 + menuBar()->sizeHint().height())));
}

void WelcomeWindow::buildMenus()
{
    AppController &app = AppController::instance();
    QMenuBar *bar = menuBar();

    QMenu *file = bar->addMenu(tr("&File"));
    file->addAction(tr("&Open…"), QKeySequence::Open, &app, &AppController::showOpenDialog);
    file->addMenu(app.recentFilesMenu(file));
    file->addSeparator();
    file->addAction(tr("&Close"), QKeySequence::Close, this, &QWidget::close);
    QAction *quit = file->addAction(tr("&Quit"), QKeySequence::Quit, &app, &AppController::quit);
    quit->setMenuRole(QAction::QuitRole);

    // Settings (one entry per Preferences tab, like ares)
    app.addSettingsMenu(bar);

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
}

void WelcomeWindow::updatePalette()
{
    // Port of update_gui_palette() in SDL/gui.c: the BMP's four palette
    // entries are replaced with the current monochrome palette's colours.
    QImage image(ResourceLocator::backgroundImagePath());
    if (image.isNull() || image.format() != QImage::Format_Indexed8) {
        image = QImage(160, 144, QImage::Format_Indexed8);
        image.setColorCount(4);
        image.fill(0);
    }
    const GB_palette_t *palette = currentUserPalette();
    QList<QRgb> colors = image.colorTable();
    colors.resize(qMax<qsizetype>(colors.size(), 4));
    for (int i = 0; i < 4; i++) {
        colors[i] = qRgb(palette->colors[i].r, palette->colors[i].g, palette->colors[i].b);
    }
    image.setColorTable(colors);
    m_background = image.convertToFormat(QImage::Format_RGB32);
    update();
}

void WelcomeWindow::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    const QRect area = centralWidget()->geometry();
    if (m_background.isNull()) {
        painter.fillRect(area, Qt::black);
        return;
    }
    // Keep the logo's aspect ratio, but fill the rest of the window with the
    // logo's own (palette-tinted) backdrop colour instead of black bars.
    painter.fillRect(area, m_background.pixelColor(0, 0));
    QSize size = m_background.size();
    size.scale(area.size(), Qt::KeepAspectRatio);
    const QRect target(area.topLeft() + QPoint((area.width() - size.width()) / 2, (area.height() - size.height()) / 2), size);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(target, m_background);
}

void WelcomeWindow::mouseDoubleClickEvent(QMouseEvent *)
{
    AppController::instance().showOpenDialog();
}

void WelcomeWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void WelcomeWindow::dropEvent(QDropEvent *event)
{
    QStringList files;
    for (const QUrl &url : event->mimeData()->urls()) {
        if (url.isLocalFile()) {
            files << url.toLocalFile();
        }
    }
    event->acceptProposedAction();
    AppController::instance().openFiles(files);
}
