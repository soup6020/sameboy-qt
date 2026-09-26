#include "CpuGraph.h"
#include "core/EmulatorSession.h"

#include <QPainter>
#include <QPainterPath>

CpuGraph::CpuGraph(EmulatorSession *session, QWidget *parent) : QWidget(parent), m_session(session)
{
    setMinimumSize(64, 16);
}

void CpuGraph::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const size_t count = EmulatorSession::kCpuSampleCount;
    const double w = width();
    const double h = height();

    QPainterPath line;
    for (size_t i = 0; i < count; i++) {
        const double sample = qBound(0.0, m_session->cpuUsageSample(i), 1.0);
        const QPointF point(w * double(i) / double(count - 1), h - (sample * (h - 1) + 0.5));
        if (i == 0) {
            line.moveTo(point);
        }
        else {
            line.lineTo(point);
        }
    }
    QPainterPath fill = line;
    fill.lineTo(w, h);
    fill.lineTo(0, h);

    // Horizontal gradient that switches between green and red per sample.
    QLinearGradient gradient(0, 0, w, 0);
    const QColor green(48, 199, 89), red(255, 69, 58);
    bool isRed = m_session->cpuUsageSample(0) >= 1;
    gradient.setColorAt(0, isRed ? red : green);
    for (size_t i = 1; i < count; i++) {
        const bool red_ = m_session->cpuUsageSample(i) >= 1;
        if (red_ != isRed) {
            const double position = double(i) / double(count - 1);
            gradient.setColorAt(qMax(0.0, position - 0.0001), isRed ? red : green);
            gradient.setColorAt(position, red_ ? red : green);
            isRed = red_;
        }
    }
    gradient.setColorAt(1, isRed ? red : green);

    painter.setClipPath(fill);
    painter.setOpacity(1 / 3.0);
    painter.fillRect(rect(), gradient);
    painter.setClipping(false);
    painter.setOpacity(1);
    painter.setPen(QPen(QBrush(gradient), 1.5));
    painter.drawPath(line);
}
