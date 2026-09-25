#include "PaletteEditorDialog.h"
#include "settings/PaletteThemes.h"
#include "settings/Settings.h"

#include <QApplication>
#include <QBoxLayout>
#include <QCheckBox>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QSlider>
#include <QToolButton>
#include <QtEndian>

#include <cmath>
#include <cstring>

namespace {

// .sbp file layout (GBPaletteEditorController.m theme_t, packed, little endian)
constexpr uint32_t kMagic = 0x5342504C; // 'SBPL' as a clang multichar constant
#pragma pack(push, 1)
struct SbpTheme {
    uint32_t magic;
    uint8_t flags; // bit 0: manual, bit 1: disabled_lcd_color
    struct { uint8_t r, g, b; } colors[5];
    int32_t brightnessBias;
    uint32_t hueBias;
    uint32_t hueBiasStrength;
};
#pragma pack(pop)
static_assert(sizeof(SbpTheme) == 5 + 15 + 12);

double blend(double from, double to, double position)
{
    return from * (1 - position) + to * position;
}

} // namespace

PaletteEditorDialog::PaletteEditorDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Palette Editor"));
    m_themesList = new QListWidget;
    m_themesList->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);

    auto *add = new QToolButton;
    add->setText(QStringLiteral("+"));
    auto *remove = new QToolButton;
    remove->setText(QStringLiteral("−"));
    auto *more = new QToolButton;
    more->setText(QStringLiteral("…"));
    more->setPopupMode(QToolButton::InstantPopup);
    auto *menu = new QMenu(more);
    menu->addAction(tr("Restore Default Palettes"), this, &PaletteEditorDialog::restoreDefaults);
    menu->addSeparator();
    menu->addAction(tr("Import Palette…"), this, &PaletteEditorDialog::importTheme);
    menu->addAction(tr("Export Palette…"), this, &PaletteEditorDialog::exportTheme);
    more->setMenu(menu);
    auto *listButtons = new QHBoxLayout;
    listButtons->addWidget(add);
    listButtons->addWidget(remove);
    listButtons->addStretch();
    listButtons->addWidget(more);
    auto *listColumn = new QVBoxLayout;
    listColumn->addWidget(m_themesList, 1);
    listColumn->addLayout(listButtons);

    auto *wells = new QHBoxLayout;
    for (int i = 0; i < 5; i++) {
        m_colorWells[i] = new QPushButton;
        m_colorWells[i]->setFixedSize(44, 28);
        connect(m_colorWells[i], &QPushButton::clicked, this, [this, i] { pickColor(i); });
        wells->addWidget(m_colorWells[i]);
    }
    m_disableLCDColorCheckbox = new QCheckBox(tr("Distinct disabled LCD color"));
    m_manualModeCheckbox = new QCheckBox(tr("Manual mode"));
    m_brightnessSlider = new QSlider(Qt::Horizontal);
    m_brightnessSlider->setRange(0, 256);
    m_brightnessSlider->setTickPosition(QSlider::TicksAbove);
    m_brightnessSlider->setTickInterval(128);
    m_hueSlider = new QSlider(Qt::Horizontal);
    m_hueSlider->setRange(0, 360);
    m_hueStrengthSlider = new QSlider(Qt::Horizontal);
    m_hueStrengthSlider->setRange(0, 256);

    auto *editor = new QFormLayout;
    editor->addRow(wells);
    editor->addRow(m_disableLCDColorCheckbox);
    editor->addRow(m_manualModeCheckbox);
    editor->addRow(tr("Brightness bias:"), m_brightnessSlider);
    editor->addRow(tr("Hue bias:"), m_hueSlider);
    editor->addRow(tr("Hue bias strength:"), m_hueStrengthSlider);

    auto *columns = new QHBoxLayout;
    columns->addLayout(listColumn, 1);
    columns->addLayout(editor, 2);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    auto *layout = new QVBoxLayout(this);
    layout->addLayout(columns);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
    connect(add, &QToolButton::clicked, this, &PaletteEditorDialog::addTheme);
    connect(remove, &QToolButton::clicked, this, &PaletteEditorDialog::deleteTheme);
    connect(m_themesList, &QListWidget::currentRowChanged, this, &PaletteEditorDialog::selectionChanged);
    connect(m_themesList, &QListWidget::itemChanged, this, [this](QListWidgetItem *item) {
        renameTheme(item->data(Qt::UserRole).toString(), item->text());
    });
    connect(m_disableLCDColorCheckbox, &QCheckBox::toggled, this, [this] {
        if (!m_loading) {
            updateEnabledControls();
            savePalette();
        }
    });
    connect(m_manualModeCheckbox, &QCheckBox::toggled, this, [this] {
        if (!m_loading) {
            updateEnabledControls();
            savePalette();
        }
    });
    for (QSlider *slider : {m_brightnessSlider, m_hueSlider, m_hueStrengthSlider}) {
        connect(slider, &QSlider::valueChanged, this, [this] {
            if (!m_loading) {
                updateAutoColors();
            }
        });
    }
    resize(640, 360);
    reload();
}

QStringList PaletteEditorDialog::sortedThemeNames() const
{
    QStringList names = Settings::instance().mapValue(QStringLiteral("GBThemes")).keys();
    std::sort(names.begin(), names.end(), [](const QString &a, const QString &b) {
        return QString::localeAwareCompare(a.toLower(), b.toLower()) < 0;
    });
    return names;
}

void PaletteEditorDialog::reload()
{
    Settings &settings = Settings::instance();
    QStringList names = sortedThemeNames();
    if (names.isEmpty()) {
        settings.setValue(QStringLiteral("GBCurrentTheme"), QStringLiteral("Untitled Palette"));
        savePalette();
        names = sortedThemeNames();
    }
    m_loading = true;
    m_themesList->clear();
    for (const QString &name : names) {
        auto *item = new QListWidgetItem(name);
        item->setData(Qt::UserRole, name);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        m_themesList->addItem(item);
    }
    const int index = int(names.indexOf(settings.stringValue(QStringLiteral("GBCurrentTheme"))));
    m_loading = false;
    m_themesList->setCurrentRow(index >= 0 ? index : 0);
    selectionChanged();
}

void PaletteEditorDialog::selectionChanged()
{
    if (m_loading || !m_themesList->currentItem()) {
        return;
    }
    Settings::instance().setValue(QStringLiteral("GBCurrentTheme"), m_themesList->currentItem()->data(Qt::UserRole).toString());
    loadPalette();
}

void PaletteEditorDialog::setColor(int index, const QColor &color)
{
    m_colors[index] = color;
    m_colorWells[index]->setStyleSheet(QStringLiteral("background-color: %1; border: 1px solid palette(mid);").arg(color.name()));
}

void PaletteEditorDialog::loadPalette()
{
    Settings &settings = Settings::instance();
    const QVariantMap theme =
        settings.mapValue(QStringLiteral("GBThemes")).value(settings.stringValue(QStringLiteral("GBCurrentTheme"))).toMap();
    m_loading = true;
    const QVariantList colors = theme.value(QStringLiteral("Colors")).toList();
    if (colors.size() == 5) {
        for (int i = 0; i < 5; i++) {
            setColor(i, themeColorFromInt(colors[i].toUInt()));
        }
    }
    m_disableLCDColorCheckbox->setChecked(theme.value(QStringLiteral("DisabledLCDColor")).toBool());
    m_manualModeCheckbox->setChecked(theme.value(QStringLiteral("Manual")).toBool());
    m_brightnessSlider->setValue(int(std::round(theme.value(QStringLiteral("BrightnessBias")).toDouble() * 128 + 128)));
    m_hueSlider->setValue(int(std::round(theme.value(QStringLiteral("HueBias")).toDouble() * 360)));
    m_hueStrengthSlider->setValue(int(std::round(theme.value(QStringLiteral("HueBiasStrength")).toDouble() * 256)));
    m_loading = false;
    updateEnabledControls();
}

void PaletteEditorDialog::savePalette()
{
    QVariantList colors;
    for (const QColor &color : m_colors) {
        colors << QVariant::fromValue<quint32>(themeColorToInt(color));
    }
    const QVariantMap theme = {
        {QStringLiteral("Colors"), colors},
        {QStringLiteral("DisabledLCDColor"), m_disableLCDColorCheckbox->isChecked()},
        {QStringLiteral("Manual"), m_manualModeCheckbox->isChecked()},
        {QStringLiteral("BrightnessBias"), (m_brightnessSlider->value() - 128) / 128.0},
        {QStringLiteral("HueBias"), m_hueSlider->value() / 360.0},
        {QStringLiteral("HueBiasStrength"), m_hueStrengthSlider->value() / 256.0},
    };
    Settings &settings = Settings::instance();
    QVariantMap themes = settings.mapValue(QStringLiteral("GBThemes"));
    themes[settings.stringValue(QStringLiteral("GBCurrentTheme"))] = theme;
    settings.setValue(QStringLiteral("GBThemes"), themes);
}

void PaletteEditorDialog::updateEnabledControls()
{
    const bool manual = m_manualModeCheckbox->isChecked();
    m_brightnessSlider->setEnabled(!manual);
    m_hueSlider->setEnabled(!manual);
    m_hueStrengthSlider->setEnabled(!manual);
    m_colorWells[1]->setEnabled(manual);
    m_colorWells[2]->setEnabled(manual);
    m_colorWells[3]->setEnabled(manual);
    if (manual) {
        m_colorWells[4]->setEnabled(m_disableLCDColorCheckbox->isChecked());
        if (!m_disableLCDColorCheckbox->isChecked()) {
            setColor(4, m_colors[3]);
        }
    }
    else {
        m_colorWells[4]->setEnabled(true);
        updateAutoColors();
    }
}

QColor PaletteEditorDialog::autoColorAtPosition(double position) const
{
    const QColor first = m_colors[0];
    const QColor second = m_colors[4];
    const double brightness = 1 / std::pow(4, (m_brightnessSlider->value() - 128) / 128.0);
    position = std::pow(position, brightness);
    const QColor hue = QColor::fromHsvF(float(std::fmod(m_hueSlider->value() / 360.0, 1.0)), 1, 1);
    const double bias = m_hueStrengthSlider->value() / 256.0;
    const double red = 1 / std::pow(4, (hue.redF() * 2 - 1) * bias);
    const double green = 1 / std::pow(4, (hue.greenF() * 2 - 1) * bias);
    const double blue = 1 / std::pow(4, (hue.blueF() * 2 - 1) * bias);
    return QColor::fromRgbF(float(blend(first.redF(), second.redF(), std::pow(position, red))),
                            float(blend(first.greenF(), second.greenF(), std::pow(position, green))),
                            float(blend(first.blueF(), second.blueF(), std::pow(position, blue))));
}

void PaletteEditorDialog::updateAutoColors()
{
    if (m_manualModeCheckbox->isChecked()) {
        savePalette();
        return;
    }
    if (m_disableLCDColorCheckbox->isChecked()) {
        setColor(1, autoColorAtPosition(8 / 25.0));
        setColor(2, autoColorAtPosition(16 / 25.0));
        setColor(3, autoColorAtPosition(24 / 25.0));
    }
    else {
        setColor(1, autoColorAtPosition(1 / 3.0));
        setColor(2, autoColorAtPosition(2 / 3.0));
        setColor(3, m_colors[4]);
    }
    savePalette();
}

void PaletteEditorDialog::pickColor(int index)
{
    const QColor color = QColorDialog::getColor(m_colors[index], this);
    if (!color.isValid()) {
        return;
    }
    setColor(index, color);
    if (index == 3 && !m_disableLCDColorCheckbox->isChecked()) {
        setColor(4, color);
    }
    updateAutoColors();
}

void PaletteEditorDialog::addTheme()
{
    Settings &settings = Settings::instance();
    const QVariantMap themes = settings.mapValue(QStringLiteral("GBThemes"));
    QString newName = QStringLiteral("Untitled Palette");
    unsigned i = 2;
    while (themes.contains(newName)) {
        newName = QStringLiteral("Untitled Palette %1").arg(i++);
    }
    settings.setValue(QStringLiteral("GBCurrentTheme"), newName);
    savePalette();
    reload();
}

void PaletteEditorDialog::deleteTheme()
{
    Settings &settings = Settings::instance();
    QVariantMap themes = settings.mapValue(QStringLiteral("GBThemes"));
    themes.remove(settings.stringValue(QStringLiteral("GBCurrentTheme")));
    settings.setValue(QStringLiteral("GBThemes"), themes);
    reload();
}

void PaletteEditorDialog::renameTheme(const QString &oldName, const QString &requested)
{
    if (m_loading || oldName == requested) {
        return;
    }
    Settings &settings = Settings::instance();
    QVariantMap themes = settings.mapValue(QStringLiteral("GBThemes"));
    const QString base = requested.isEmpty() ? QStringLiteral("Untitled Palette") : requested;
    QString newName = base;
    unsigned i = 2;
    while (themes.contains(newName)) {
        newName = QStringLiteral("%1 %2").arg(base).arg(i++);
    }
    themes[newName] = themes.take(oldName);
    if (settings.stringValue(QStringLiteral("GBCurrentTheme")) == oldName) {
        settings.setValue(QStringLiteral("GBCurrentTheme"), newName);
    }
    settings.setValue(QStringLiteral("GBThemes"), themes);
    QMetaObject::invokeMethod(this, &PaletteEditorDialog::reload, Qt::QueuedConnection);
}

void PaletteEditorDialog::exportTheme()
{
    const QString name = Settings::instance().stringValue(QStringLiteral("GBCurrentTheme"));
    QString path = QFileDialog::getSaveFileName(this, tr("Export Palette"), name + QStringLiteral(".sbp"),
                                                tr("SameBoy Palette (*.sbp)"));
    if (path.isEmpty()) {
        return;
    }
    SbpTheme theme{};
    theme.magic = qToLittleEndian(kMagic);
    theme.flags = uint8_t((m_manualModeCheckbox->isChecked() ? 1 : 0) | (m_disableLCDColorCheckbox->isChecked() ? 2 : 0));
    for (int i = 0; i < 5; i++) {
        theme.colors[i] = {uint8_t(m_colors[i].red()), uint8_t(m_colors[i].green()), uint8_t(m_colors[i].blue())};
    }
    theme.brightnessBias = qToLittleEndian(int32_t((m_brightnessSlider->value() - 128) * (0x40000000 / 128)));
    theme.hueBias = qToLittleEndian(uint32_t(std::round(m_hueSlider->value() * (0x80000000 / 360.0))));
    theme.hueBiasStrength = qToLittleEndian(uint32_t(m_hueStrengthSlider->value() * (0x80000000 / 256)));
    size_t size = sizeof(theme);
    if (m_manualModeCheckbox->isChecked()) {
        size = m_disableLCDColorCheckbox->isChecked() ? 5 + 5 * 3 : 5 + 4 * 3;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(reinterpret_cast<const char *>(&theme), qint64(size)) != qint64(size)) {
        QApplication::beep();
    }
}

void PaletteEditorDialog::importTheme()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Import Palette"), QString(), tr("SameBoy Palette (*.sbp)"));
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QApplication::beep();
        return;
    }
    const QByteArray data = file.readAll();
    SbpTheme theme{};
    memcpy(&theme, data.constData(), qMin(size_t(data.size()), sizeof(theme)));
    if (qFromLittleEndian(theme.magic) != kMagic) {
        QApplication::beep();
        return;
    }
    m_loading = true;
    m_manualModeCheckbox->setChecked(theme.flags & 1);
    m_disableLCDColorCheckbox->setChecked(theme.flags & 2);
    for (int i = 0; i < 5; i++) {
        setColor(i, QColor(theme.colors[i].r, theme.colors[i].g, theme.colors[i].b));
    }
    if (!(theme.flags & 2)) {
        setColor(4, m_colors[3]);
    }
    m_brightnessSlider->setValue(int(std::round(int32_t(qFromLittleEndian(theme.brightnessBias)) / (0x40000000 / 128.0) + 128)));
    m_hueSlider->setValue(int(std::round(qFromLittleEndian(theme.hueBias) / (0x80000000 / 360.0))));
    m_hueStrengthSlider->setValue(int(std::round(qFromLittleEndian(theme.hueBiasStrength) / (0x80000000 / 256.0))));
    m_loading = false;

    Settings &settings = Settings::instance();
    const QVariantMap themes = settings.mapValue(QStringLiteral("GBThemes"));
    const QString baseName = QFileInfo(path).completeBaseName();
    QString newName = baseName;
    unsigned i = 2;
    while (themes.contains(newName)) {
        newName = QStringLiteral("%1 %2").arg(baseName).arg(i++);
    }
    settings.setValue(QStringLiteral("GBCurrentTheme"), newName);
    savePalette();
    reload();
}

void PaletteEditorDialog::restoreDefaults()
{
    Settings::instance().updateThemesDefault(true);
    reload();
}
