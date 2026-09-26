#include "MemoryViewer.h"
#include "HexView.h"
#include "core/EmulatorSession.h"
#include "settings/Settings.h"

#include <QApplication>
#include <QBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QToolTip>

MemoryViewer::MemoryViewer(EmulatorSession *session, QWidget *parent)
    : QWidget(parent, Qt::Window), m_session(session), m_model(session)
{
    m_spaceButton = new QComboBox;
    m_spaceButton->addItems({tr("Entire Space"), tr("ROM"), tr("Video RAM"), tr("Cartridge RAM"), tr("RAM")});
    m_bankInput = new QLineEdit;
    m_bankInput->setPlaceholderText(tr("Bank"));
    m_bankInput->setMaximumWidth(80);
    m_bankInput->setEnabled(false);
    m_goToInput = new QLineEdit;
    m_goToInput->setPlaceholderText(tr("Address"));
    m_goToInput->setMaximumWidth(200);

    auto *toolbar = new QHBoxLayout;
    toolbar->addWidget(new QLabel(tr("Address Space:")));
    toolbar->addWidget(m_spaceButton);
    toolbar->addWidget(new QLabel(tr("Bank:")));
    toolbar->addWidget(m_bankInput);
    toolbar->addStretch();
    toolbar->addWidget(new QLabel(tr("Go To:")));
    toolbar->addWidget(m_goToInput);

    m_hexView = new HexView(&m_model);
    m_status = new QLabel;
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(toolbar);
    layout->addWidget(m_hexView, 1);
    layout->addWidget(m_status);

    connect(m_spaceButton, &QComboBox::currentIndexChanged, this, &MemoryViewer::updateSpace);
    connect(m_bankInput, &QLineEdit::returnPressed, this, [this] { updateBank(false); });
    connect(m_goToInput, &QLineEdit::returnPressed, this, &MemoryViewer::goTo);
    connect(m_hexView, &HexView::cursorMoved, this, &MemoryViewer::updateStatus);
    connect(session, &EmulatorSession::consoleOutput, this, [this] {
        if (isVisible()) {
            m_hexView->reload();
        }
    });

    m_refreshTimer.setInterval(250);
    connect(&m_refreshTimer, &QTimer::timeout, m_hexView, &HexView::reload);

    Settings &settings = Settings::instance();
    settings.observe(this, QStringLiteral("GBDebuggerFont"), [this](const QVariant &) { updateFont(); });
    settings.observe(this, QStringLiteral("GBDebuggerFontSize"), [this](const QVariant &) { updateFont(); });
    resize(m_hexView->minimumWidth() + 40, 480);
    updateStatus();
}

void MemoryViewer::updateFont()
{
    QFont font(Settings::instance().stringValue(QStringLiteral("GBDebuggerFont")));
    font.setStyleHint(QFont::Monospace);
    font.setFixedPitch(true);
    font.setPointSize(12);
    m_hexView->setFont(font);
}

void MemoryViewer::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    m_refreshTimer.start();
    m_hexView->reload();
}

void MemoryViewer::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    m_refreshTimer.stop();
}

void MemoryViewer::showError(QWidget *anchor, const QString &error)
{
    QApplication::beep();
    QToolTip::showText(anchor->mapToGlobal(QPoint(0, anchor->height())), error, anchor, {}, 5000);
}

void MemoryViewer::updateSpace(int index)
{
    // Port of -[Document hexUpdateSpace:]
    const auto mode = MemoryModel::Mode(index);
    m_bankInput->setEnabled(mode != MemoryModel::EntireSpace);
    m_model.setMode(mode);
    uint16_t bank = uint16_t(-1);
    GB_gameboy_t *gb = m_session->gb();
    switch (mode) {
        case MemoryModel::EntireSpace: break;
        case MemoryModel::ROM: GB_get_direct_access(gb, GB_DIRECT_ACCESS_ROM, nullptr, &bank); break;
        case MemoryModel::VRAM: GB_get_direct_access(gb, GB_DIRECT_ACCESS_VRAM, nullptr, &bank); break;
        case MemoryModel::ExternalRAM: GB_get_direct_access(gb, GB_DIRECT_ACCESS_CART_RAM, nullptr, &bank); break;
        case MemoryModel::RAM: GB_get_direct_access(gb, GB_DIRECT_ACCESS_RAM, nullptr, &bank); break;
    }
    m_model.setSelectedBank(bank);
    m_bankForDescription = bank;
    m_bankInput->setText(bank == uint16_t(-1) ? QString() : QStringLiteral("$%1").arg(bank, 0, 16));
    m_hexView->setCursorOffset(0);
    m_hexView->reload();
    updateStatus();
}

void MemoryViewer::updateBank(bool ignoreErrors)
{
    // Port of -[Document hexUpdateBank:ignoreErrors:]
    const QByteArray expression = m_bankInput->text().toUtf8();
    uint16_t address = 0, bank = 0;
    bool fail = false;
    const QString error = m_session->captureOutput(
        [&] { fail = GB_debugger_evaluate(m_session->gb(), expression.constData(), &address, &bank); });
    if (!error.isEmpty() && !ignoreErrors) {
        showError(m_bankInput, error);
    }
    if (fail) {
        return;
    }
    if (bank == uint16_t(-1)) {
        bank = address;
    }
    uint16_t banks = 1;
    GB_gameboy_t *gb = m_session->gb();
    switch (m_model.mode()) {
        case MemoryModel::ROM: {
            size_t size = 0;
            GB_get_direct_access(gb, GB_DIRECT_ACCESS_ROM, &size, nullptr);
            banks = uint16_t(size / 0x4000);
            break;
        }
        case MemoryModel::VRAM: banks = GB_is_cgb(gb) ? 2 : 1; break;
        case MemoryModel::ExternalRAM: {
            size_t size = 0;
            GB_get_direct_access(gb, GB_DIRECT_ACCESS_CART_RAM, &size, nullptr);
            banks = uint16_t((size + 0x1FFF) / 0x2000);
            break;
        }
        case MemoryModel::RAM: banks = GB_is_cgb(gb) ? 8 : 1; break;
        case MemoryModel::EntireSpace: break;
    }
    bank %= qMax<uint16_t>(banks, 1);
    m_model.setSelectedBank(bank);
    m_bankForDescription = bank;
    m_bankInput->setText(QStringLiteral("$%1").arg(bank, 0, 16));
    m_hexView->reload();
    updateStatus();
}

void MemoryViewer::sessionReset()
{
    m_model.setSelectedBank(0);
    updateBank(true);
}

void MemoryViewer::goTo()
{
    // Port of -[Document hexGoTo:]
    const QByteArray expression = m_goToInput->text().toUtf8();
    uint16_t address = 0, bank = 0;
    bool fail = false;
    const QString error = m_session->captureOutput(
        [&] { fail = GB_debugger_evaluate(m_session->gb(), expression.constData(), &address, &bank); });
    if (!error.isEmpty()) {
        showError(m_goToInput, error);
    }
    if (fail) {
        return;
    }
    if (bank != uint16_t(-1)) {
        MemoryModel::Mode mode = m_model.mode();
        if (address < 0x4000) {
            if (bank == 0) {
                if (mode != MemoryModel::ROM && mode != MemoryModel::EntireSpace) {
                    mode = MemoryModel::EntireSpace;
                }
            }
            else {
                address |= 0x4000;
                mode = MemoryModel::ROM;
            }
        }
        else if (address < 0x8000) {
            mode = MemoryModel::ROM;
        }
        else if (address < 0xA000) {
            mode = MemoryModel::VRAM;
        }
        else if (address < 0xC000) {
            mode = MemoryModel::ExternalRAM;
        }
        else if (address < 0xD000) {
            if (mode != MemoryModel::RAM && mode != MemoryModel::EntireSpace) {
                mode = MemoryModel::EntireSpace;
            }
        }
        else if (address < 0xE000) {
            mode = MemoryModel::RAM;
        }
        else {
            mode = MemoryModel::EntireSpace;
        }
        m_spaceButton->setCurrentIndex(mode);
        updateSpace(mode);
        m_bankInput->setText(QStringLiteral("$%1").arg(bank, 2, 16, QLatin1Char('0')));
        updateBank(false);
    }
    const uint16_t offset = uint16_t(address - m_model.base());
    if (offset >= m_model.length()) {
        GB_log(m_session->gb(), "Value $%04x is out of range.\n", offset);
        return;
    }
    m_hexView->setCursorOffset(offset);
    m_hexView->setFocus();
}

void MemoryViewer::updateStatus()
{
    const size_t offset = m_hexView->cursorOffset();
    const uint16_t address = uint16_t(offset + m_model.base());
    QString text = QStringLiteral("$%1").arg(address, 4, 16, QLatin1Char('0'));
    if (GB_is_inited(m_session->gb())) {
        const uint16_t bank = offset < 0x4000 ? uint16_t(-1) : m_bankForDescription;
        const char *description = GB_debugger_describe_address(m_session->gb(), address, bank, false, false);
        if (description && *description) {
            text += QStringLiteral(" (%1)").arg(QString::fromUtf8(description));
        }
    }
    m_status->setText(text);
}
