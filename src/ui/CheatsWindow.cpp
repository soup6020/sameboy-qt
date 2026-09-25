#include "CheatsWindow.h"
#include "core/EmulatorSession.h"

#include <QApplication>
#include <QBoxLayout>
#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStyle>
#include <QTableWidget>
#include <QToolButton>
#include <QToolTip>

namespace {

const GB_cheat_t *const *cheatList(GB_gameboy_t *gb, size_t *count)
{
    *count = 0;
    if (!GB_is_inited(gb)) {
        return nullptr;
    }
    return GB_get_cheats(gb, count);
}

uint8_t parseValue(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.startsWith(QLatin1Char('$'))) {
        return uint8_t(trimmed.mid(1).toUInt(nullptr, 16));
    }
    return uint8_t(trimmed.toInt());
}

} // namespace

QString CheatsWindow::addressString(const GB_cheat_t *cheat)
{
    if (cheat->bank != GB_CHEAT_ANY_BANK) {
        return QStringLiteral("$%1:$%2").arg(cheat->bank, 0, 16).arg(cheat->address, 4, 16, QLatin1Char('0'));
    }
    return QStringLiteral("$%1").arg(cheat->address, 4, 16, QLatin1Char('0'));
}

QString CheatsWindow::actionDescription(const GB_cheat_t *cheat)
{
    if (cheat->use_old_value) {
        return QStringLiteral("[%1]($%2) = $%3")
            .arg(addressString(cheat))
            .arg(cheat->old_value, 2, 16, QLatin1Char('0'))
            .arg(cheat->value, 2, 16, QLatin1Char('0'));
    }
    return QStringLiteral("[%1] = $%2").arg(addressString(cheat)).arg(cheat->value, 2, 16, QLatin1Char('0'));
}

CheatsWindow::CheatsWindow(EmulatorSession *session, QWidget *parent)
    : QWidget(parent, Qt::Window), m_session(session)
{
    m_table = new QTableWidget(0, 4);
    m_table->setHorizontalHeaderLabels({QString(), tr("Enabled"), tr("Description"), tr("Action")});
    m_table->verticalHeader()->hide();
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);

    m_addressField = new QLineEdit;
    m_valueField = new QLineEdit;
    m_oldValueCheckbox = new QCheckBox(tr("Only if old value was:"));
    m_oldValueField = new QLineEdit;
    m_descriptionField = new QLineEdit;
    m_descriptionField->setPlaceholderText(tr("Description"));

    auto *editor = new QGroupBox;
    auto *form = new QFormLayout(editor);
    form->addRow(tr("Description"), m_descriptionField);
    auto *actionRow = new QHBoxLayout;
    actionRow->addWidget(new QLabel(tr("Change byte at address:")));
    actionRow->addWidget(m_addressField);
    actionRow->addWidget(new QLabel(tr("To value:")));
    actionRow->addWidget(m_valueField);
    form->addRow(tr("Action"), actionRow);
    auto *oldValueRow = new QHBoxLayout;
    oldValueRow->addWidget(m_oldValueCheckbox);
    oldValueRow->addWidget(m_oldValueField);
    oldValueRow->addStretch();
    form->addRow(QString(), oldValueRow);

    m_importCodeField = new QLineEdit;
    m_importCodeField->setPlaceholderText(tr("Code"));
    m_importDescriptionField = new QLineEdit;
    m_importDescriptionField->setPlaceholderText(tr("Description"));
    auto *importButton = new QPushButton(tr("Import"));
    auto *importRow = new QHBoxLayout;
    importRow->addWidget(new QLabel(tr("Import GameShark or Game Genie cheat:")));
    importRow->addWidget(m_importCodeField);
    importRow->addWidget(m_importDescriptionField);
    importRow->addWidget(importButton);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_table, 1);
    layout->addWidget(editor);
    layout->addLayout(importRow);
    resize(640, 420);

    connect(m_table, &QTableWidget::itemSelectionChanged, this, &CheatsWindow::selectionChanged);
    connect(m_table, &QTableWidget::cellClicked, this, [this](int row, int column) {
        if (column == 1) {
            toggleEnabled(row);
        }
    });
    for (QLineEdit *field : {m_addressField, m_valueField, m_oldValueField, m_descriptionField}) {
        connect(field, &QLineEdit::textEdited, this, &CheatsWindow::updateCheat);
    }
    connect(m_oldValueCheckbox, &QCheckBox::toggled, this, [this] {
        if (!m_updatingEditors) {
            updateCheat();
        }
    });
    connect(importButton, &QPushButton::clicked, this, &CheatsWindow::importCheat);
    connect(m_importCodeField, &QLineEdit::returnPressed, this, &CheatsWindow::importCheat);
    connect(session, &EmulatorSession::cheatsChanged, this, [this] {
        reload();
        selectionChanged();
    });
    reload();
    selectRow(0);
}

void CheatsWindow::reload()
{
    size_t count = 0;
    const GB_cheat_t *const *cheats = cheatList(m_session->gb(), &count);
    const int selected = m_table->currentRow();
    QSignalBlocker blocker(m_table);
    m_table->setRowCount(int(count) + 1);
    for (int row = 0; row <= int(count); row++) {
        if (row < int(count)) {
            auto *deleteButton = new QToolButton;
            deleteButton->setIcon(style()->standardIcon(QStyle::SP_DialogDiscardButton));
            deleteButton->setAutoRaise(true);
            deleteButton->setToolTip(tr("Delete"));
            connect(deleteButton, &QToolButton::clicked, this, [this, row] { removeCheat(row); });
            m_table->setCellWidget(row, 0, deleteButton);
            auto *enabled = new QTableWidgetItem;
            enabled->setCheckState(cheats[row]->enabled ? Qt::Checked : Qt::Unchecked);
            m_table->setItem(row, 1, enabled);
            m_table->setItem(row, 2, new QTableWidgetItem(QString::fromUtf8(cheats[row]->description)));
            m_table->setItem(row, 3, new QTableWidgetItem(actionDescription(cheats[row])));
        }
        else {
            m_table->removeCellWidget(row, 0);
            m_table->setItem(row, 0, new QTableWidgetItem);
            auto *enabled = new QTableWidgetItem;
            enabled->setCheckState(Qt::Unchecked);
            m_table->setItem(row, 1, enabled);
            auto *add = new QTableWidgetItem(tr("Add Cheat…"));
            QFont italic = add->font();
            italic.setItalic(true);
            add->setFont(italic);
            m_table->setItem(row, 2, add);
            m_table->setItem(row, 3, new QTableWidgetItem);
        }
    }
    if (selected >= 0 && selected < m_table->rowCount()) {
        m_table->selectRow(selected);
    }
}

void CheatsWindow::selectRow(int row)
{
    reload();
    m_table->selectRow(qBound(0, row, m_table->rowCount() - 1));
    selectionChanged();
}

void CheatsWindow::selectionChanged()
{
    size_t count = 0;
    const GB_cheat_t *const *cheats = cheatList(m_session->gb(), &count);
    const int row = m_table->currentRow();
    static const GB_cheat_t templateCheat = [] {
        GB_cheat_t cheat{};
        cheat.bank = 0;
        strcpy(cheat.description, "New Cheat");
        return cheat;
    }();
    const GB_cheat_t *cheat = (row >= 0 && row < int(count)) ? cheats[row] : &templateCheat;
    m_updatingEditors = true;
    m_addressField->setText(addressString(cheat));
    m_valueField->setText(QStringLiteral("$%1").arg(cheat->value, 2, 16, QLatin1Char('0')));
    m_oldValueField->setText(QStringLiteral("$%1").arg(cheat->old_value, 2, 16, QLatin1Char('0')));
    m_oldValueCheckbox->setChecked(cheat->use_old_value);
    m_descriptionField->setText(QString::fromUtf8(cheat->description));
    m_updatingEditors = false;
}

void CheatsWindow::updateCheat()
{
    // Port of -[GBCheatWindowController updateCheat:]
    GB_gameboy_t *gb = m_session->gb();
    uint16_t address = 0;
    uint16_t bank = GB_CHEAT_ANY_BANK;
    const QString addressText = m_addressField->text().trimmed();
    if (addressText.contains(QLatin1Char(':'))) {
        bank = uint16_t(addressText.section(QLatin1Char(':'), 0, 0).remove(QLatin1Char('$')).toUInt(nullptr, 16));
        address = uint16_t(addressText.section(QLatin1Char(':'), 1).remove(QLatin1Char('$')).toUInt(nullptr, 16));
    }
    else {
        address = uint16_t(QString(addressText).remove(QLatin1Char('$')).toUInt(nullptr, 16));
    }
    const uint8_t value = parseValue(m_valueField->text());
    const uint8_t oldValue = parseValue(m_oldValueField->text());
    const QByteArray description = m_descriptionField->text().toUtf8();
    const bool useOldValue = m_oldValueCheckbox->isChecked();

    size_t count = 0;
    const GB_cheat_t *const *cheats = cheatList(gb, &count);
    const int row = m_table->currentRow();
    m_session->performAtomic([&] {
        if (row < 0 || row >= int(count)) {
            GB_add_cheat(gb, description.constData(), address, bank, value, oldValue, useOldValue, false);
        }
        else {
            GB_update_cheat(gb, cheats[row], description.constData(), address, bank, value, oldValue, useOldValue,
                            cheats[row]->enabled);
        }
    });
    reload();
    if (row < 0 || row >= int(count)) {
        m_table->selectRow(int(count));
    }
}

void CheatsWindow::toggleEnabled(int row)
{
    GB_gameboy_t *gb = m_session->gb();
    size_t count = 0;
    const GB_cheat_t *const *cheats = cheatList(gb, &count);
    m_session->performAtomic([&] {
        if (row >= int(count)) {
            GB_add_cheat(gb, "New Cheat", 0, 0, 0, 0, false, true);
        }
        else {
            const GB_cheat_t *cheat = cheats[row];
            GB_update_cheat(gb, cheat, cheat->description, cheat->address, cheat->bank, cheat->value, cheat->old_value,
                            cheat->use_old_value, !cheat->enabled);
        }
    });
    reload();
    selectionChanged();
}

void CheatsWindow::removeCheat(int row)
{
    GB_gameboy_t *gb = m_session->gb();
    size_t count = 0;
    const GB_cheat_t *const *cheats = cheatList(gb, &count);
    if (row >= int(count)) {
        return;
    }
    m_session->performAtomic([&] { GB_remove_cheat(gb, cheats[row]); });
    reload();
    selectionChanged();
}

void CheatsWindow::importCheat()
{
    GB_gameboy_t *gb = m_session->gb();
    const QByteArray code = m_importCodeField->text().toUtf8();
    const QByteArray description = m_importDescriptionField->text().toUtf8();
    bool success = false;
    m_session->performAtomic([&] { success = GB_import_cheat(gb, code.constData(), description.constData(), true); });
    if (success) {
        m_importCodeField->clear();
        m_importDescriptionField->clear();
        reload();
        selectionChanged();
    }
    else {
        QApplication::beep();
        QToolTip::showText(m_importCodeField->mapToGlobal(QPoint(0, m_importCodeField->height())),
                           tr("This code is not a valid GameShark or Game Genie code"), m_importCodeField, {}, 5000);
    }
}
