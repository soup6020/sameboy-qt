#include "DebuggerConsole.h"
#include "ConsoleInput.h"
#include "CpuGraph.h"
#include "settings/Settings.h"

#include <QBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QSplitter>
#include <QStyle>
#include <QTextEdit>
#include <QToolButton>

namespace {

QFont debuggerFont(int size = 0)
{
    Settings &settings = Settings::instance();
    QFont font(settings.stringValue(QStringLiteral("GBDebuggerFont")));
    font.setStyleHint(QFont::Monospace);
    font.setFixedPitch(true);
    font.setPointSize(size ? size : settings.intValue(QStringLiteral("GBDebuggerFontSize")));
    return font;
}

void styleTerminal(QWidget *widget)
{
    QPalette palette = widget->palette();
    palette.setColor(QPalette::Base, QColor(0x1e, 0x1e, 0x1e));
    palette.setColor(QPalette::Text, Qt::white);
    widget->setPalette(palette);
}

void appendChunk(QTextEdit *view, const LogChunk &chunk, const QFont &font)
{
    QTextCursor cursor(view->document());
    cursor.movePosition(QTextCursor::End);
    QTextCharFormat format;
    QFont chunkFont = font;
    chunkFont.setBold(chunk.attributes & GB_LOG_BOLD);
    format.setFont(chunkFont);
    format.setForeground(Qt::white);
    if (chunk.attributes & GB_LOG_UNDERLINE_MASK) {
        format.setFontUnderline(true);
        if ((chunk.attributes & GB_LOG_UNDERLINE_MASK) == GB_LOG_DASHED_UNDERLINE) {
            format.setUnderlineStyle(QTextCharFormat::DotLine);
        }
    }
    cursor.insertText(chunk.text, format);
}

} // namespace

DebuggerConsole::DebuggerConsole(EmulatorSession *session, QWidget *parent)
    : QWidget(parent, Qt::Window), m_session(session)
{
    resize(960, 480);

    m_output = new QTextEdit;
    m_output->setReadOnly(true);
    m_output->setUndoRedoEnabled(false);
    m_output->setLineWrapMode(QTextEdit::WidgetWidth);
    styleTerminal(m_output);

    m_input = new ConsoleInput(session->gb());
    styleTerminal(m_input);
    connect(m_input, &ConsoleInput::commandEntered, this, &DebuggerConsole::consoleInput);

    auto makeButton = [this](const QString &title, QStyle::StandardPixmap icon, const QString &command) {
        auto *button = new QToolButton;
        button->setText(title);
        button->setToolTip(title);
        button->setIcon(style()->standardIcon(icon));
        button->setAutoRaise(true);
        button->setProperty("command", command);
        connect(button, &QToolButton::clicked, this, [this, button] {
            m_session->queueDebuggerCommand(button->property("command").toString());
        });
        return button;
    };
    m_continueButton = makeButton(tr("Continue"), QStyle::SP_MediaPlay, QStringLiteral("continue"));
    m_finishButton = makeButton(tr("Step Out"), QStyle::SP_ArrowUp, QStringLiteral("finish"));
    m_nextButton = makeButton(tr("Step Over"), QStyle::SP_ArrowForward, QStringLiteral("next"));
    m_backstepButton = makeButton(tr("Step Backward"), QStyle::SP_ArrowBack, QStringLiteral("backstep"));
    m_stepButton = makeButton(tr("Step Into"), QStyle::SP_ArrowDown, QStringLiteral("step"));
    QToolButton *helpButton = makeButton(tr("Help"), QStyle::SP_DialogHelpButton, QStringLiteral("help"));

    m_cpuGraph = new CpuGraph(session);
    m_cpuGraph->setFixedWidth(120);
    m_cpuLabel = new QLabel(QStringLiteral("0.00%"));
    m_cpuLabel->setMinimumWidth(m_cpuLabel->fontMetrics().horizontalAdvance(QStringLiteral("100.00%")));
    m_cpuLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto *bar = new QHBoxLayout;
    bar->setContentsMargins(4, 2, 4, 2);
    for (QToolButton *button : {m_continueButton, m_finishButton, m_nextButton, m_backstepButton, m_stepButton, helpButton}) {
        bar->addWidget(button);
    }
    bar->addStretch();
    bar->addWidget(new QLabel(tr("CPU Load")));
    bar->addWidget(m_cpuGraph);
    bar->addWidget(m_cpuLabel);

    auto *mainPane = new QWidget;
    auto *mainLayout = new QVBoxLayout(mainPane);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(m_output, 1);
    mainLayout->addLayout(bar);
    mainLayout->addWidget(m_input);

    m_sideInput = new QPlainTextEdit;
    m_sideInput->setPlainText(QStringLiteral("registers\nbacktrace\n"));
    styleTerminal(m_sideInput);
    m_sideOutput = new QTextEdit;
    m_sideOutput->setReadOnly(true);
    styleTerminal(m_sideOutput);
    auto *sideSplitter = new QSplitter(Qt::Vertical);
    sideSplitter->addWidget(m_sideInput);
    sideSplitter->addWidget(m_sideOutput);
    sideSplitter->setStretchFactor(1, 4);

    m_splitter = new QSplitter(Qt::Horizontal);
    m_splitter->addWidget(mainPane);
    m_splitter->addWidget(sideSplitter);
    m_splitter->setCollapsible(0, false);
    m_splitter->setCollapsible(1, true);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setSizes({640, 320});

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_splitter);

    session->setSideViewCommands(m_sideInput->toPlainText().split(QLatin1Char('\n')));
    m_sideViewTimer.setSingleShot(true);
    m_sideViewTimer.setInterval(300);
    connect(m_sideInput, &QPlainTextEdit::textChanged, this, [this] {
        m_session->setSideViewCommands(m_sideInput->toPlainText().split(QLatin1Char('\n')));
        m_sideViewTimer.start();
    });
    connect(&m_sideViewTimer, &QTimer::timeout, m_session, &EmulatorSession::refreshSideView);

    connect(session, &EmulatorSession::debuggerStateChanged, this, &DebuggerConsole::updateButtons);
    connect(session, &EmulatorSession::runningChanged, this, &DebuggerConsole::updateButtons);

    m_cpuTimer.setInterval(250);
    connect(&m_cpuTimer, &QTimer::timeout, this, &DebuggerConsole::updateCpuUsage);

    Settings &settings = Settings::instance();
    settings.observe(this, QStringLiteral("GBDebuggerFont"), [this](const QVariant &) { updateFonts(); });
    settings.observe(this, QStringLiteral("GBDebuggerFontSize"), [this](const QVariant &) { updateFonts(); });
    updateButtons();
}

void DebuggerConsole::updateFonts()
{
    const QFont font = debuggerFont();
    for (QWidget *widget : std::initializer_list<QWidget *>{m_input, m_sideInput}) {
        widget->setFont(font);
    }
    // Re-apply the family/size to existing output while keeping bold runs.
    for (QTextEdit *view : {m_output, m_sideOutput}) {
        view->setFont(font);
        QTextCursor cursor(view->document());
        cursor.select(QTextCursor::Document);
        QTextCharFormat format;
        format.setFontFamilies({font.family()});
        format.setFontPointSize(font.pointSizeF());
        cursor.mergeCharFormat(format);
    }
}

void DebuggerConsole::appendOutput(const QList<LogChunk> &chunks, bool clearSideView)
{
    if (clearSideView) {
        m_sideOutput->clear();
    }
    const QFont font = debuggerFont();
    bool mainChanged = false;
    for (const LogChunk &chunk : chunks) {
        appendChunk(chunk.sideView ? m_sideOutput : m_output, chunk, font);
        mainChanged |= !chunk.sideView;
    }
    if (mainChanged) {
        m_output->verticalScrollBar()->setValue(m_output->verticalScrollBar()->maximum());
    }
}

void DebuggerConsole::clear()
{
    m_output->clear();
}

void DebuggerConsole::focusInput()
{
    m_input->setFocus();
    updateCpuUsage();
}

void DebuggerConsole::consoleInput(const QString &text)
{
    QString line = text;
    if (line.isEmpty() && !m_lastInput.isEmpty()) {
        line = m_lastInput;
    }
    else if (!line.isEmpty()) {
        m_lastInput = line;
    }
    m_session->queueDebuggerCommand(line);
}

void DebuggerConsole::updateButtons()
{
    const bool paused = m_session->isPaused();
    if (paused) {
        m_continueButton->setText(tr("Continue"));
        m_continueButton->setToolTip(tr("Continue"));
        m_continueButton->setProperty("command", QStringLiteral("continue"));
        m_continueButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        m_input->setPlaceholderText(QString());
    }
    else {
        m_continueButton->setText(tr("Interrupt"));
        m_continueButton->setToolTip(tr("Interrupt"));
        m_continueButton->setProperty("command", QStringLiteral("interrupt"));
        m_continueButton->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    }
    for (QToolButton *button : {m_nextButton, m_stepButton, m_finishButton, m_backstepButton}) {
        button->setEnabled(paused);
    }
}

void DebuggerConsole::updateCpuUsage()
{
    const double usage = GB_debugger_get_second_cpu_usage(m_session->gb());
    m_cpuLabel->setText(QStringLiteral("%1%").arg(usage * 100, 0, 'f', 2));
    m_cpuGraph->update();
}

void DebuggerConsole::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    m_cpuTimer.start();
    updateCpuUsage();
    emit visibilityChanged(true);
}

void DebuggerConsole::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    m_cpuTimer.stop();
    emit visibilityChanged(false);
}
