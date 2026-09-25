#pragma once

#include <QLineEdit>
#include <QStringList>

extern "C" {
#include <Core/gb.h>
}

// Port of Cocoa/GBTerminalTextFieldCell: command history (↑/↓), reverse
// search (Ctrl+R) and debugger tab completion.
class ConsoleInput : public QLineEdit
{
    Q_OBJECT

public:
    explicit ConsoleInput(GB_gameboy_t *gb, QWidget *parent = nullptr);

signals:
    void commandEntered(const QString &command);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    void complete();
    void updateReverseSearch(const QString &needle);

    GB_gameboy_t *m_gb;
    QStringList m_lines;
    int m_currentLine = 0;
    bool m_reverseSearchMode = false;
    QString m_reverseSearchNeedle;
    int m_autoCompleteStart = -1;
    int m_autoCompleteLength = 0;
    uintptr_t m_autoCompleteContext = 0;
};
