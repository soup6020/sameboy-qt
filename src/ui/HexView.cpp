#include "HexView.h"
#include "MemoryModel.h"

#include <QKeyEvent>
#include <QPainter>
#include <QScrollBar>

#include <vector>

HexView::HexView(MemoryModel *model, QWidget *parent) : QAbstractScrollArea(parent), m_model(model)
{
    setFocusPolicy(Qt::StrongFocus);
    viewport()->setCursor(Qt::IBeamCursor);
    updateMetrics();
}

void HexView::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::FontChange) {
        updateMetrics();
    }
    QAbstractScrollArea::changeEvent(event);
}

void HexView::updateMetrics()
{
    const QFontMetrics metrics(font());
    m_charWidth = metrics.horizontalAdvance(QLatin1Char('0'));
    m_lineHeight = metrics.height() + 2;
    m_addressWidth = m_charWidth * 6;
    m_hexX = m_addressWidth + m_charWidth;
    m_asciiX = m_hexX + m_charWidth * (kBytesPerLine * 3 + 1);
    setMinimumWidth(m_asciiX + m_charWidth * (kBytesPerLine + 2) + verticalScrollBar()->sizeHint().width());
    updateScrollBar();
    viewport()->update();
}

int HexView::visibleLines() const
{
    return qMax(1, viewport()->height() / m_lineHeight);
}

void HexView::updateScrollBar()
{
    const int lines = int((m_model->length() + kBytesPerLine - 1) / kBytesPerLine);
    verticalScrollBar()->setRange(0, qMax(0, lines - visibleLines()));
    verticalScrollBar()->setPageStep(visibleLines());
}

void HexView::resizeEvent(QResizeEvent *event)
{
    QAbstractScrollArea::resizeEvent(event);
    updateScrollBar();
}

void HexView::reload()
{
    updateScrollBar();
    if (m_cursor >= m_model->length()) {
        m_cursor = 0;
    }
    viewport()->update();
}

void HexView::setCursorOffset(size_t offset, bool ensureVisible)
{
    if (m_model->length() == 0) {
        return;
    }
    m_cursor = qMin(offset, m_model->length() - 1);
    m_highNibbleDone = false;
    if (ensureVisible) {
        const int line = int(m_cursor / kBytesPerLine);
        QScrollBar *bar = verticalScrollBar();
        if (line < bar->value()) {
            bar->setValue(line);
        }
        else if (line >= bar->value() + visibleLines()) {
            bar->setValue(line - visibleLines() + 1);
        }
    }
    viewport()->update();
    emit cursorMoved(m_cursor);
}

void HexView::paintEvent(QPaintEvent *)
{
    QPainter painter(viewport());
    painter.setFont(font());
    const QPalette pal = palette();
    painter.fillRect(viewport()->rect(), pal.base());
    painter.fillRect(QRect(0, 0, m_addressWidth + m_charWidth / 2, viewport()->height()), pal.alternateBase());

    const int firstLine = verticalScrollBar()->value();
    const int lines = visibleLines() + 1;
    const size_t start = size_t(firstLine) * kBytesPerLine;
    if (start >= m_model->length()) {
        return;
    }
    const size_t count = qMin(size_t(lines * kBytesPerLine), m_model->length() - start);
    std::vector<uint8_t> bytes(count);
    m_model->read(start, count, bytes.data());

    const QFontMetrics metrics(font());
    const int ascent = metrics.ascent() + 1;
    const QColor dim = pal.color(QPalette::PlaceholderText);
    for (int line = 0; line < lines; line++) {
        const size_t lineStart = start + size_t(line) * kBytesPerLine;
        if (lineStart >= m_model->length()) {
            break;
        }
        const int y = line * m_lineHeight;
        painter.setPen(dim);
        painter.drawText(m_charWidth / 2, y + ascent,
                         QStringLiteral("%1").arg(lineStart + m_model->base(), 4, 16, QLatin1Char('0')).toUpper());
        for (int column = 0; column < kBytesPerLine; column++) {
            const size_t offset = lineStart + size_t(column);
            if (offset >= m_model->length()) {
                break;
            }
            const uint8_t value = bytes[offset - start];
            const int hexX = m_hexX + column * 3 * m_charWidth + (column >= 8 ? m_charWidth : 0);
            const int asciiX = m_asciiX + column * m_charWidth;
            if (offset == m_cursor) {
                const QColor highlight = pal.color(hasFocus() ? QPalette::Highlight : QPalette::Mid);
                QColor faint = highlight;
                faint.setAlphaF(0.35);
                painter.fillRect(QRect(hexX - 1, y, m_charWidth * 2 + 2, m_lineHeight),
                                 m_asciiColumn ? faint : highlight);
                painter.fillRect(QRect(asciiX, y, m_charWidth, m_lineHeight), m_asciiColumn ? highlight : faint);
            }
            painter.setPen(offset == m_cursor && !m_asciiColumn ? pal.color(QPalette::HighlightedText)
                                                                : pal.color(QPalette::Text));
            painter.drawText(hexX, y + ascent, QStringLiteral("%1").arg(value, 2, 16, QLatin1Char('0')).toUpper());
            const QChar character = (value >= 0x20 && value < 0x7F) ? QChar(value) : QLatin1Char('.');
            painter.setPen(offset == m_cursor && m_asciiColumn ? pal.color(QPalette::HighlightedText)
                                                               : pal.color(QPalette::Text));
            painter.drawText(asciiX, y + ascent, QString(character));
        }
    }
}

void HexView::mousePressEvent(QMouseEvent *event)
{
    const QPoint pos = event->pos();
    const size_t line = size_t(verticalScrollBar()->value()) + size_t(pos.y() / m_lineHeight);
    int column = -1;
    if (pos.x() >= m_asciiX) {
        column = (pos.x() - m_asciiX) / m_charWidth;
        m_asciiColumn = true;
    }
    else if (pos.x() >= m_hexX) {
        int x = pos.x() - m_hexX;
        if (x >= 8 * 3 * m_charWidth) {
            x -= m_charWidth;
        }
        column = x / (3 * m_charWidth);
        m_asciiColumn = false;
    }
    if (column >= 0 && column < kBytesPerLine) {
        setCursorOffset(line * kBytesPerLine + size_t(column), false);
    }
    setFocus();
}

void HexView::writeByte(size_t offset, uint8_t value)
{
    m_model->write(offset, &value, 1);
}

void HexView::keyPressEvent(QKeyEvent *event)
{
    const size_t length = m_model->length();
    switch (event->key()) {
        case Qt::Key_Left:
            if (m_cursor > 0) {
                setCursorOffset(m_cursor - 1);
            }
            return;
        case Qt::Key_Right: setCursorOffset(m_cursor + 1); return;
        case Qt::Key_Up:
            if (m_cursor >= kBytesPerLine) {
                setCursorOffset(m_cursor - kBytesPerLine);
            }
            return;
        case Qt::Key_Down: setCursorOffset(m_cursor + kBytesPerLine); return;
        case Qt::Key_PageUp:
            setCursorOffset(m_cursor > size_t(visibleLines()) * kBytesPerLine
                                ? m_cursor - size_t(visibleLines()) * kBytesPerLine
                                : 0);
            return;
        case Qt::Key_PageDown: setCursorOffset(m_cursor + size_t(visibleLines()) * kBytesPerLine); return;
        case Qt::Key_Home:
            setCursorOffset(event->modifiers() & Qt::ControlModifier ? 0 : m_cursor - m_cursor % kBytesPerLine);
            return;
        case Qt::Key_End:
            setCursorOffset(event->modifiers() & Qt::ControlModifier
                                ? length - 1
                                : m_cursor - m_cursor % kBytesPerLine + kBytesPerLine - 1);
            return;
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            m_asciiColumn = !m_asciiColumn;
            m_highNibbleDone = false;
            viewport()->update();
            return;
    }
    const QString text = event->text();
    if (text.size() != 1 || (event->modifiers() & (Qt::ControlModifier | Qt::MetaModifier))) {
        QAbstractScrollArea::keyPressEvent(event);
        return;
    }
    const QChar c = text.at(0);
    if (m_asciiColumn) {
        if (c.unicode() >= 0x20 && c.unicode() < 0x7F) {
            writeByte(m_cursor, uint8_t(c.unicode()));
            setCursorOffset(m_cursor + 1);
        }
        return;
    }
    bool ok = false;
    const int nibble = QString(c).toInt(&ok, 16);
    if (!ok) {
        return;
    }
    uint8_t value = 0;
    m_model->read(m_cursor, 1, &value);
    if (!m_highNibbleDone) {
        value = uint8_t((value & 0x0F) | (nibble << 4));
        writeByte(m_cursor, value);
        m_highNibbleDone = true;
        viewport()->update();
    }
    else {
        value = uint8_t((value & 0xF0) | nibble);
        writeByte(m_cursor, value);
        setCursorOffset(m_cursor + 1);
    }
}
