#include "PrinterWindow.h"
#include "core/EmulatorSession.h"

#include <QApplication>
#include <QBoxLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QPainter>
#include <QPrintDialog>
#include <QPrinter>
#include <QProgressBar>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>

PrinterWindow::PrinterWindow(EmulatorSession *session, QWidget *parent)
    : QWidget(parent, Qt::Window), m_session(session)
{
    auto *saveButton = new QPushButton(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Save"));
    auto *printButton = new QPushButton(tr("Print…"));
    m_spinner = new QProgressBar;
    m_spinner->setRange(0, 0);
    m_spinner->setMaximumWidth(80);
    m_spinner->setTextVisible(false);
    m_spinner->hide();

    m_imageLabel = new QLabel;
    m_imageLabel->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    m_scroll = new QScrollArea;
    m_scroll->setWidget(m_imageLabel);
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *toolbar = new QHBoxLayout;
    toolbar->addWidget(m_spinner);
    toolbar->addStretch();
    toolbar->addWidget(saveButton);
    toolbar->addWidget(printButton);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(toolbar);
    layout->addWidget(m_scroll, 1);

    connect(saveButton, &QPushButton::clicked, this, &PrinterWindow::save);
    connect(printButton, &QPushButton::clicked, this, &PrinterWindow::print);
    setMinimumWidth(160 * 2 + 40);
}

void PrinterWindow::appendImage(const QImage &chunk)
{
    if (!isVisible()) {
        m_feed = QImage();
    }
    if (m_feed.isNull()) {
        m_feed = chunk;
    }
    else {
        QImage combined(160, m_feed.height() + chunk.height(), QImage::Format_RGBX8888);
        QPainter painter(&combined);
        painter.drawImage(0, 0, m_feed);
        painter.drawImage(0, m_feed.height(), chunk);
        painter.end();
        m_feed = combined;
    }
    m_spinner->show();
    m_imageLabel->setPixmap(QPixmap::fromImage(m_feed.scaled(m_feed.size() * 2, Qt::IgnoreAspectRatio, Qt::FastTransformation)));
    const int maxHeight = screen() ? screen()->availableGeometry().height() * 3 / 4 : 800;
    resize(width(), qMin(maxHeight, m_feed.height() * 2 + 80));
    m_scroll->verticalScrollBar()->setValue(m_scroll->verticalScrollBar()->maximum());
}

void PrinterWindow::printingDone()
{
    m_spinner->hide();
}

void PrinterWindow::save()
{
    if (m_feed.isNull()) {
        QApplication::beep();
        return;
    }
    const bool wasRunning = m_session->isRunning();
    m_session->stop();
    QString path = QFileDialog::getSaveFileName(this, tr("Save Printer Feed"), QString(), tr("PNG Image (*.png)"));
    if (!path.isEmpty()) {
        if (QFileInfo(path).suffix().isEmpty()) {
            path += QStringLiteral(".png");
        }
        if (m_feed.save(path, "PNG")) {
            hide();
        }
        else {
            QApplication::beep();
        }
    }
    if (wasRunning) {
        m_session->start();
    }
}

void PrinterWindow::print()
{
    if (m_feed.isNull()) {
        QApplication::beep();
        return;
    }
    QPrinter printer;
    QPrintDialog dialog(&printer, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    QPainter painter(&printer);
    const QRect page = painter.viewport();
    // Print at 2x like Cocoa's image view, shrinking to fit the page if needed.
    QSize size = m_feed.size() * 2;
    size.scale(size.boundedTo(page.size()), Qt::KeepAspectRatio);
    painter.drawImage(QRect(QPoint(0, 0), size), m_feed);
}
