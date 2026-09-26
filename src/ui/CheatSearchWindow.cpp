#include "CheatSearchWindow.h"
#include "core/EmulatorSession.h"

#include <QApplication>
#include <QBoxLayout>
#include <QComboBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QTableWidget>
#include <QToolTip>

CheatSearchWindow::CheatSearchWindow(EmulatorSession *session, QWidget *parent)
    : QWidget(parent, Qt::Window), m_session(session)
{
    m_dataTypeButton = new QComboBox;
    m_dataTypeButton->addItem(tr("8-Bit"), int(GB_CHEAT_SEARCH_DATA_TYPE_8BIT));
    m_dataTypeButton->addItem(tr("16-Bit"), int(GB_CHEAT_SEARCH_DATA_TYPE_16BIT));
    m_dataTypeButton->addItem(tr("16-Bit (Big Endian)"), int(GB_CHEAT_SEARCH_DATA_TYPE_16BIT_BE));
    m_conditionTypeButton = new QComboBox;
    m_conditionTypeButton->addItems({tr("Any"), tr("Is Equal To…"), tr("Is Different From…"), tr("Is Greater Than…"),
                                     tr("Is Equal or Greater Than…"), tr("Is Less Than…"), tr("Is Equal or Less Than…"),
                                     tr("Did Change"), tr("Did Not Change"), tr("Did Increase"), tr("Did Decrease"),
                                     tr("Custom…")});
    m_operandField = new QLineEdit(QStringLiteral("$0"));
    m_conditionField = new QLineEdit(QStringLiteral("1"));
    m_resultsLabel = new QLabel;

    auto *searchButton = new QPushButton(tr("Search"));
    searchButton->setDefault(true);
    auto *resetButton = new QPushButton(tr("Reset"));
    m_addCheatButton = new QPushButton(tr("Add Cheat"));
    m_addCheatButton->setEnabled(false);

    m_table = new QTableWidget(0, 3);
    m_table->setHorizontalHeaderLabels({tr("Address"), tr("Previous Value"), tr("Current Value")});
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);

    auto *form = new QFormLayout;
    form->addRow(tr("Data Type:"), m_dataTypeButton);
    auto *conditionRow = new QHBoxLayout;
    conditionRow->addWidget(m_conditionTypeButton);
    conditionRow->addWidget(m_operandField);
    form->addRow(tr("Search Condition:"), conditionRow);
    form->addRow(tr("Expression:"), m_conditionField);

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(resetButton);
    buttons->addWidget(m_resultsLabel, 1);
    buttons->addWidget(m_addCheatButton);
    buttons->addWidget(searchButton);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(m_table, 1);
    layout->addLayout(buttons);
    resize(480, 480);

    connect(m_conditionTypeButton, &QComboBox::currentIndexChanged, this, &CheatSearchWindow::conditionChanged);
    connect(m_operandField, &QLineEdit::textEdited, this, &CheatSearchWindow::conditionChanged);
    connect(m_operandField, &QLineEdit::returnPressed, this, &CheatSearchWindow::search);
    connect(m_conditionField, &QLineEdit::returnPressed, this, &CheatSearchWindow::search);
    connect(searchButton, &QPushButton::clicked, this, &CheatSearchWindow::search);
    connect(resetButton, &QPushButton::clicked, this, &CheatSearchWindow::reset);
    connect(m_addCheatButton, &QPushButton::clicked, this, &CheatSearchWindow::addCheat);
    connect(m_table, &QTableWidget::itemSelectionChanged, this,
            [this] { m_addCheatButton->setEnabled(!m_table->selectedItems().isEmpty()); });
    connect(m_table, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item) {
        if (m_updatingTable || item->column() != 2) {
            return;
        }
        // Write the evaluated expression into memory (tableView:setObjectValue:)
        const QByteArray expression = item->text().toUtf8();
        const int row = item->row();
        uint16_t value = 0;
        bool success = false;
        const QString error = m_session->captureOutput(
            [&] { success = !GB_debugger_evaluate(m_session->gb(), expression.constData(), &value, nullptr); });
        if (!success) {
            QApplication::beep();
            QToolTip::showText(QCursor::pos(), error, m_table, {}, 5000);
        }
        else {
            m_session->performAtomic([&] {
                uint8_t *dest = addressForRow(row);
                if (dataType() & GB_CHEAT_SEARCH_DATA_TYPE_BE_BIT) {
                    value = uint16_t((value >> 8) | (value << 8));
                }
                dest[0] = uint8_t(value);
                if (dataType() & GB_CHEAT_SEARCH_DATA_TYPE_16BIT) {
                    dest[1] = uint8_t(value >> 8);
                }
            });
        }
        reloadCurrentValues();
    });

    m_refreshTimer.setInterval(250);
    connect(&m_refreshTimer, &QTimer::timeout, this, &CheatSearchWindow::reloadCurrentValues);
    conditionChanged();
}

GB_cheat_search_data_type_t CheatSearchWindow::dataType() const
{
    return GB_cheat_search_data_type_t(m_dataTypeButton->currentData().toInt());
}

void CheatSearchWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    m_refreshTimer.start();
}

void CheatSearchWindow::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    m_refreshTimer.stop();
}

void CheatSearchWindow::reset()
{
    m_dataTypeButton->setEnabled(true);
    m_session->performAtomic([this] { GB_cheat_search_reset(m_session->gb()); });
    m_results.clear();
    m_table->setRowCount(0);
    m_resultsLabel->clear();
}

void CheatSearchWindow::conditionChanged()
{
    const int index = m_conditionTypeButton->currentIndex();
    m_conditionField->setEnabled(index == 11);
    m_operandField->setEnabled(index >= 1 && index <= 6);
    const QString operand = m_operandField->text();
    switch (index) {
        case 0: m_conditionField->setText(QStringLiteral("1")); break;
        case 1: m_conditionField->setText(QStringLiteral("new == (%1)").arg(operand)); break;
        case 2: m_conditionField->setText(QStringLiteral("new != (%1)").arg(operand)); break;
        case 3: m_conditionField->setText(QStringLiteral("new > (%1)").arg(operand)); break;
        case 4: m_conditionField->setText(QStringLiteral("new >= (%1)").arg(operand)); break;
        case 5: m_conditionField->setText(QStringLiteral("new < (%1)").arg(operand)); break;
        case 6: m_conditionField->setText(QStringLiteral("new <= (%1)").arg(operand)); break;
        case 7: m_conditionField->setText(QStringLiteral("new != old")); break;
        case 8: m_conditionField->setText(QStringLiteral("new == old")); break;
        case 9: m_conditionField->setText(QStringLiteral("new > old")); break;
        case 10: m_conditionField->setText(QStringLiteral("new < old")); break;
    }
}

void CheatSearchWindow::search()
{
    m_dataTypeButton->setEnabled(false);
    const QByteArray expression = m_conditionField->text().toUtf8();
    const GB_cheat_search_data_type_t type = dataType();
    bool success = false;
    const QString error = m_session->captureOutput(
        [&] { success = GB_cheat_search_filter(m_session->gb(), expression.constData(), type); });
    if (!success) {
        QApplication::beep();
        QToolTip::showText(m_conditionField->mapToGlobal(QPoint(0, m_conditionField->height())), error,
                           m_conditionField, {}, 5000);
        return;
    }
    m_session->performAtomic([&] {
        m_results.resize(GB_cheat_search_result_count(m_session->gb()));
        GB_cheat_search_get_results(m_session->gb(), m_results.data());
    });
    if (m_results.empty()) {
        m_dataTypeButton->setEnabled(true);
        m_resultsLabel->setText(tr("No results."));
    }
    else {
        m_resultsLabel->setText(tr("%1 result%2")
                                    .arg(QLocale().toString(qulonglong(m_results.size())),
                                         m_results.size() > 1 ? QStringLiteral("s") : QString()));
    }

    m_updatingTable = true;
    // Displaying millions of rows is pointless; Cocoa's table is lazy, ours caps.
    const int rows = int(qMin<size_t>(m_results.size(), 10000));
    m_table->setRowCount(rows);
    const bool sixteen = type & GB_CHEAT_SEARCH_DATA_TYPE_16BIT;
    for (int row = 0; row < rows; row++) {
        const auto &result = m_results[size_t(row)];
        auto *address = new QTableWidgetItem(QStringLiteral("$%1:$%2")
                                                 .arg(result.bank, 2, 16, QLatin1Char('0'))
                                                 .arg(result.addr, 4, 16, QLatin1Char('0')));
        address->setFlags(address->flags() & ~Qt::ItemIsEditable);
        auto *previous =
            new QTableWidgetItem(QStringLiteral("$%1").arg(result.value, sixteen ? 4 : 2, 16, QLatin1Char('0')));
        previous->setFlags(previous->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, 0, address);
        m_table->setItem(row, 1, previous);
        m_table->setItem(row, 2, new QTableWidgetItem);
    }
    m_updatingTable = false;
    reloadCurrentValues();
}

uint8_t *CheatSearchWindow::addressForRow(int row) const
{
    const auto &result = m_results[size_t(row)];
    GB_gameboy_t *gb = m_session->gb();
    uint8_t *base = nullptr;
    uint32_t offset = 0;
    if (result.addr < 0xC000) {
        base = static_cast<uint8_t *>(GB_get_direct_access(gb, GB_DIRECT_ACCESS_CART_RAM, nullptr, nullptr));
        offset = (result.addr & 0x1FFF) + result.bank * 0x2000;
    }
    else if (result.addr < 0xE000) {
        base = static_cast<uint8_t *>(GB_get_direct_access(gb, GB_DIRECT_ACCESS_RAM, nullptr, nullptr));
        offset = (result.addr & 0xFFF) + result.bank * 0x1000;
    }
    else {
        base = static_cast<uint8_t *>(GB_get_direct_access(gb, GB_DIRECT_ACCESS_HRAM, nullptr, nullptr));
        offset = result.addr & 0x7F;
    }
    return base + offset;
}

void CheatSearchWindow::reloadCurrentValues()
{
    // Don't overwrite a value while the user is editing it.
    if (QWidget *focus = QApplication::focusWidget(); focus && focus != m_table && m_table->isAncestorOf(focus)) {
        return;
    }
    m_updatingTable = true;
    const GB_cheat_search_data_type_t type = dataType();
    for (int row = 0; row < m_table->rowCount(); row++) {
        const uint8_t *data = addressForRow(row);
        uint16_t value = data[0];
        QString text;
        if (!(type & GB_CHEAT_SEARCH_DATA_TYPE_16BIT)) {
            text = QStringLiteral("$%1").arg(value, 2, 16, QLatin1Char('0'));
        }
        else {
            value |= uint16_t(data[1] << 8);
            if (type & GB_CHEAT_SEARCH_DATA_TYPE_BE_BIT) {
                value = uint16_t((value >> 8) | (value << 8));
            }
            text = QStringLiteral("$%1").arg(value, 4, 16, QLatin1Char('0'));
        }
        if (QTableWidgetItem *item = m_table->item(row, 2); item && item->text() != text) {
            item->setText(text);
        }
    }
    m_updatingTable = false;
}

void CheatSearchWindow::addCheat()
{
    const int row = m_table->currentRow();
    if (row < 0 || size_t(row) >= m_results.size()) {
        return;
    }
    const GB_cheat_search_result_t result = m_results[size_t(row)];
    const uint8_t *data = addressForRow(row);
    const GB_cheat_search_data_type_t type = dataType();
    size_t rowToSelect = 0;
    GB_get_cheats(m_session->gb(), &rowToSelect);
    m_session->performAtomic([&] {
        GB_gameboy_t *gb = m_session->gb();
        const bool sixteen = type & GB_CHEAT_SEARCH_DATA_TYPE_16BIT;
        GB_add_cheat(gb, sixteen ? "New Cheat (Part 1)" : "New Cheat", result.addr, result.bank, data[0], 0, false,
                     true);
        if (sixteen) {
            GB_add_cheat(gb, "New Cheat (Part 2)", uint16_t(result.addr + 1), result.bank, data[1], 0, false, true);
        }
        GB_set_cheats_enabled(gb, true);
    });
    emit cheatAdded(int(rowToSelect));
}
