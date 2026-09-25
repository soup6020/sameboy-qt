#include "ConsoleInput.h"

#include <QApplication>
#include <QKeyEvent>
#include <QPainter>

ConsoleInput::ConsoleInput(GB_gameboy_t *gb, QWidget *parent)
    : QLineEdit(parent), m_gb(gb)
{
    connect(this, &QLineEdit::returnPressed, this, [this] {
        const QString line = text();
        if (!line.isEmpty()) {
            m_lines.removeAll(line);
            m_lines.append(line);
        }
        m_currentLine = int(m_lines.size());
        m_reverseSearchMode = false;
        clear();
        emit commandEntered(line);
    });
    connect(this, &QLineEdit::cursorPositionChanged, this, [this] {
        if (m_autoCompleteStart >= 0 && cursorPosition() != m_autoCompleteStart + m_autoCompleteLength) {
            m_autoCompleteContext = 0;
            m_autoCompleteStart = -1;
        }
    });
}

bool ConsoleInput::event(QEvent *event)
{
    // Tab must complete rather than move focus.
    if (event->type() == QEvent::KeyPress && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Tab) {
        complete();
        return true;
    }
    return QLineEdit::event(event);
}

void ConsoleInput::complete()
{
    if (!m_autoCompleteContext || m_autoCompleteStart < 0) {
        if (hasSelectedText()) {
            del();
        }
        m_autoCompleteStart = cursorPosition();
        m_autoCompleteLength = 0;
        m_autoCompleteContext = 0;
    }
    QByteArray prefix = text().left(m_autoCompleteStart).toUtf8();
    uintptr_t context = m_autoCompleteContext;
    char *completion = GB_debugger_complete_substring(m_gb, prefix.data(), &context);
    if (completion) {
        const QString string = QString::fromUtf8(completion);
        free(completion);
        QString current = text();
        current.replace(m_autoCompleteStart, m_autoCompleteLength, string);
        const int start = m_autoCompleteStart;
        blockSignals(true);
        setText(current);
        setCursorPosition(start + int(string.size()));
        blockSignals(false);
        m_autoCompleteStart = start;
        m_autoCompleteLength = int(string.size());
        m_autoCompleteContext = context;
        return;
    }
    m_autoCompleteContext = context;
    QApplication::beep();
}

void ConsoleInput::keyPressEvent(QKeyEvent *event)
{
    const bool control = (event->modifiers() & (Qt::ControlModifier | Qt::MetaModifier)) &&
                         !(event->modifiers() & (Qt::AltModifier | Qt::ShiftModifier));
    if (control && event->key() == Qt::Key_R) {
        if (m_lines.isEmpty()) {
            QApplication::beep();
            return;
        }
        if (!m_reverseSearchMode) {
            m_reverseSearchMode = true;
            m_reverseSearchNeedle.clear();
            m_currentLine = int(m_lines.size()) - 1;
            clear();
        }
        else if (m_currentLine > 0) {
            m_currentLine--;
        }
        else {
            QApplication::beep();
        }
        if (!m_reverseSearchNeedle.isEmpty()) {
            updateReverseSearch(m_reverseSearchNeedle);
        }
        update();
        return;
    }
    if (m_reverseSearchMode) {
        if (event->key() == Qt::Key_Backspace) {
            m_reverseSearchNeedle.chop(1);
            m_currentLine = int(m_lines.size()) - 1;
            if (m_reverseSearchNeedle.isEmpty()) {
                clear();
            }
            else {
                updateReverseSearch(m_reverseSearchNeedle);
            }
            update();
            return;
        }
        if (!event->text().isEmpty() && event->text().at(0).isPrint()) {
            m_reverseSearchNeedle += event->text();
            updateReverseSearch(m_reverseSearchNeedle);
            return;
        }
        if (event->key() != Qt::Key_Return && event->key() != Qt::Key_Enter) {
            m_reverseSearchMode = false;
            update();
        }
    }
    switch (event->key()) {
        case Qt::Key_Up:
            m_reverseSearchMode = false;
            if (m_currentLine > 0) {
                m_currentLine--;
                setText(m_lines[m_currentLine]);
            }
            else {
                setCursorPosition(0);
                QApplication::beep();
            }
            return;
        case Qt::Key_Down:
            m_reverseSearchMode = false;
            if (m_currentLine >= m_lines.size()) {
                clear();
                QApplication::beep();
                return;
            }
            m_currentLine++;
            if (m_currentLine == m_lines.size()) {
                clear();
            }
            else {
                setText(m_lines[m_currentLine]);
            }
            return;
    }
    QLineEdit::keyPressEvent(event);
}

void ConsoleInput::updateReverseSearch(const QString &needle)
{
    for (int line = m_currentLine; line >= 0; line--) {
        const int index = int(m_lines[line].indexOf(needle));
        if (index >= 0) {
            m_currentLine = line;
            setText(m_lines[line]);
            setSelection(index, int(needle.size()));
            return;
        }
    }
    QApplication::beep();
}

void ConsoleInput::focusOutEvent(QFocusEvent *event)
{
    m_reverseSearchMode = false;
    QLineEdit::focusOutEvent(event);
}

void ConsoleInput::paintEvent(QPaintEvent *event)
{
    QLineEdit::paintEvent(event);
    if (m_reverseSearchMode && text().isEmpty()) {
        QPainter painter(this);
        QColor color = palette().color(QPalette::Text);
        color.setAlphaF(0.5);
        painter.setPen(color);
        painter.drawText(rect().adjusted(6, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, tr("Reverse search..."));
    }
}
