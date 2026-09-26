#pragma once

#include "core/EmulatorSession.h"

#include <QTimer>
#include <QWidget>

class ConsoleInput;
class CpuGraph;
class QLabel;
class QPlainTextEdit;
class QSplitter;
class QTextEdit;
class QToolButton;

// The Debug Console window from Document.xib: output, prompt, stepping
// buttons, CPU meter and a side view of commands re-run on every break.
class DebuggerConsole : public QWidget
{
    Q_OBJECT

public:
    explicit DebuggerConsole(EmulatorSession *session, QWidget *parent = nullptr);

    void appendOutput(const QList<LogChunk> &chunks, bool clearSideView);
    void clear();
    void focusInput();

signals:
    void visibilityChanged(bool visible);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void updateFonts();
    void updateButtons();
    void updateCpuUsage();
    void consoleInput(const QString &text);

    EmulatorSession *m_session;
    QTextEdit *m_output;
    QTextEdit *m_sideOutput;
    QPlainTextEdit *m_sideInput;
    ConsoleInput *m_input;
    QSplitter *m_splitter;
    QToolButton *m_continueButton;
    QToolButton *m_nextButton;
    QToolButton *m_stepButton;
    QToolButton *m_finishButton;
    QToolButton *m_backstepButton;
    CpuGraph *m_cpuGraph;
    QLabel *m_cpuLabel;
    QString m_lastInput;
    QTimer m_cpuTimer;
    QTimer m_sideViewTimer;
};
