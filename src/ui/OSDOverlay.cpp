#include "OSDOverlay.h"
#include "settings/Settings.h"

#include <QPainter>
#include <QPainterPath>
#include <QWidget>

OSDOverlay::OSDOverlay(QWidget *host)
    : QObject(host), m_host(host)
{
    m_timer.setInterval(25);
    connect(&m_timer, &QTimer::timeout, this, &OSDOverlay::animate);
}

void OSDOverlay::displayText(const QString &text)
{
    if (!Settings::instance().boolValue(QStringLiteral("GBOSDEnabled"))) {
        return;
    }
    m_text = text;
    m_opacity = 1.0;
    m_animation = 2.5;
    // Longer strings should appear longer
    if (m_text.contains(QLatin1Char('\n'))) {
        m_animation += 4;
    }
    m_timer.start();
    emit changed();
}

void OSDOverlay::animate()
{
    m_animation -= 0.1;
    if (m_animation < 1.0) {
        m_opacity = qMax(0.0, m_animation);
        emit changed();
    }
    if (m_animation <= 0) {
        m_timer.stop();
        m_text.clear();
        emit changed();
    }
}

void OSDOverlay::paint(QPainter &painter, const QRect &screenRect) const
{
    if (m_text.isEmpty()) {
        return;
    }
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setOpacity(m_opacity);

    double fontSize = 8;
    if (m_usesSGBScale) {
        fontSize *= qMin(screenRect.width() / 256.0, screenRect.height() / 224.0);
    }
    else {
        fontSize *= qMin(screenRect.width() / 160.0, screenRect.height() / 144.0);
    }
    QFont font = m_host->font();
    font.setBold(true);
    font.setPixelSize(qMax(1, int(fontSize)));

    const QStringList lines = m_text.split(QLatin1Char('\n'));
    const QFontMetricsF metrics(font);
    const double lineHeight = metrics.lineSpacing();
    // Cocoa draws from the bottom-left, inset by one font size.
    double y = screenRect.bottom() - fontSize - lineHeight * (lines.size() - 1) - metrics.descent();
    for (const QString &line : lines) {
        QPainterPath path;
        path.addText(QPointF(screenRect.left() + fontSize, y), font, line);
        // Outer stroke instead of an inside stroke, as GBOSDView does manually.
        painter.strokePath(path, QPen(Qt::black, qMax(2.0, fontSize / 4), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.fillPath(path, Qt::white);
        y += lineHeight;
    }
    painter.restore();
}
