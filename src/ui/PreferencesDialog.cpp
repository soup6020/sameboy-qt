#include "PreferencesDialog.h"
#include "PaletteEditorDialog.h"
#include "core/Models.h"
#include "input/GamepadManager.h"
#include "settings/Settings.h"

#include <QBoxLayout>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QFileDialog>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>

extern "C" {
#include <Core/gb.h>
}

namespace {

QString keyDisplayName(int key)
{
    switch (key) {
        case Qt::Key_Shift: return QObject::tr("Shift");
        case Qt::Key_Control: return QObject::tr("Control");
        case Qt::Key_Alt: return QObject::tr("Alt");
        case Qt::Key_Meta: return QObject::tr("Meta");
        case Qt::Key_AltGr: return QObject::tr("AltGr");
        case Qt::Key_CapsLock: return QObject::tr("Caps Lock");
        case 0: return QString();
    }
    return QKeySequence(key).toString(QKeySequence::NativeText);
}

// Hotkey actions for GBJoypadHotkey1/2 (Cocoa stores menu key equivalents).
QList<std::pair<QString, QVariant>> hotkeyActions()
{
    QList<std::pair<QString, QVariant>> items = {
        {QObject::tr("None"), QString()},
        {QObject::tr("Toggle Pause"), QStringLiteral("togglePause")},
        {QObject::tr("Reset"), QStringLiteral("reset")},
        {QObject::tr("Toggle Mute"), QStringLiteral("mute")},
    };
    for (int i = 1; i <= 10; i++) {
        items.append({QObject::tr("Save State to Slot %1").arg(i), QStringLiteral("saveState:%1").arg(i)});
    }
    for (int i = 1; i <= 10; i++) {
        items.append({QObject::tr("Load State from Slot %1").arg(i), QStringLiteral("loadState:%1").arg(i)});
    }
    return items;
}

} // namespace

PreferencesDialog::PreferencesDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Preferences"));
    auto *tabs = new QTabWidget;
    tabs->addTab(createEmulationTab(), tr("Emulation"));
    tabs->addTab(createVideoTab(), tr("Video"));
    tabs->addTab(createAudioTab(), tr("Audio"));
    tabs->addTab(createControlsTab(), tr("Controls"));
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(tabs);
    m_debounce.start();
}

// MARK: - Binding helpers (GBPreferenceButton / GBPreferencePopUpButton / GBPreferencesSlider)

QCheckBox *PreferencesDialog::checkBox(const QString &title, const QString &key, bool invert)
{
    auto *box = new QCheckBox(title);
    Settings::instance().observe(box, key, [box, invert](const QVariant &value) {
        QSignalBlocker blocker(box);
        box->setChecked(value.toBool() ^ invert);
    });
    connect(box, &QCheckBox::toggled, box, [key, invert](bool checked) {
        Settings::instance().setValue(key, checked ^ invert);
    });
    return box;
}

QComboBox *PreferencesDialog::comboBox(const QString &key, const QList<std::pair<QString, QVariant>> &items,
                                       const QList<QVariant> &disabled)
{
    auto *combo = new QComboBox;
    for (const auto &[title, value] : items) {
        combo->addItem(title, value);
        if (disabled.contains(value)) {
            combo->setItemData(combo->count() - 1, 0, Qt::UserRole - 1); // Disable item
        }
    }
    Settings::instance().observe(combo, key, [combo](const QVariant &value) {
        QSignalBlocker blocker(combo);
        for (int i = 0; i < combo->count(); i++) {
            if (combo->itemData(i).toString() == value.toString()) {
                combo->setCurrentIndex(i);
                return;
            }
        }
    });
    connect(combo, &QComboBox::activated, combo, [combo, key](int index) {
        Settings::instance().setValue(key, combo->itemData(index));
    });
    return combo;
}

QSlider *PreferencesDialog::slider(const QString &key, int minimum, int maximum, double denominator)
{
    auto *slider = new QSlider(Qt::Horizontal);
    slider->setRange(minimum, maximum);
    Settings::instance().observe(slider, key, [slider, denominator](const QVariant &value) {
        QSignalBlocker blocker(slider);
        slider->setValue(int(std::round(value.toDouble() * denominator)));
    });
    connect(slider, &QSlider::valueChanged, slider, [key, denominator](int value) {
        Settings::instance().setValue(key, value / denominator);
    });
    return slider;
}

// MARK: - Emulation

QWidget *PreferencesDialog::createEmulationTab()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);

    // Unsupported revisions are listed but disabled, as in Preferences.xib.
    form->addRow(tr("Game Boy revision:"),
                 comboBox(QStringLiteral("GBDMGModel"),
                          {{tr("DMG-CPU 0"), 0x000}, {tr("DMG-CPU A"), 0x001}, {tr("DMG-CPU B"), int(GB_MODEL_DMG_B)},
                           {tr("DMG-CPU C"), 0x003}},
                          {0x000, 0x001, 0x003}));
    form->addRow(tr("Game Boy Color revision:"),
                 comboBox(QStringLiteral("GBCGBModel"),
                          {{tr("CPU CGB 0"), int(GB_MODEL_CGB_0)}, {tr("CPU CGB A"), int(GB_MODEL_CGB_A)},
                           {tr("CPU CGB B"), int(GB_MODEL_CGB_B)}, {tr("CPU CGB C"), int(GB_MODEL_CGB_C)},
                           {tr("CPU CGB D"), int(GB_MODEL_CGB_D)}, {tr("CPU CGB E"), int(GB_MODEL_CGB_E)}}));
    form->addRow(tr("Game Boy Advance revision:"),
                 comboBox(QStringLiteral("GBAGBModel"),
                          {{tr("CPU AGB 0 (Early GBA)"), 0x206}, {tr("CPU AGB A (GBA)"), int(GB_MODEL_AGB_A)},
                           {tr("CPU AGB A (Game Boy Player)"), int(GB_MODEL_GBP_A)}, {tr("CPU AGB B (GBA SP)"), 0x208},
                           {tr("CPU AGB E (Late GBA SP)"), 0x209}, {tr("CPU AGB E (Late Game Boy Player)"), 0x229}},
                          {0x206, 0x208, 0x209, 0x229}));
    form->addRow(tr("Super Game Boy model:"),
                 comboBox(QStringLiteral("GBSGBModel"),
                          {{tr("Super Game Boy (NTSC)"), int(GB_MODEL_SGB_NTSC)},
                           {tr("Super Game Boy (PAL)"), int(GB_MODEL_SGB_PAL)},
                           {tr("Super Game Boy 2"), int(GB_MODEL_SGB2)}}));

    m_bootROMsButton = new QComboBox;
    connect(m_bootROMsButton, &QComboBox::activated, this, [this](int index) {
        const QString action = m_bootROMsButton->itemData(index).toString();
        if (action == QLatin1String("builtin")) {
            Settings::instance().remove(QStringLiteral("GBBootROMsFolder"));
        }
        else if (action == QLatin1String("other")) {
            const QString folder = QFileDialog::getExistingDirectory(
                this, tr("Select Boot ROMs Folder"), Settings::instance().stringValue(QStringLiteral("GBBootROMsFolder")));
            if (!folder.isEmpty()) {
                Settings::instance().setValue(QStringLiteral("GBBootROMsFolder"), folder);
            }
        }
        updateBootROMsMenu();
    });
    updateBootROMsMenu();
    form->addRow(tr("Boot ROMs location:"), m_bootROMsButton);

    form->addRow(tr("Rewinding duration:"),
                 comboBox(QStringLiteral("GBRewindLength"),
                          {{tr("Disabled"), 0}, {tr("10 Seconds"), 10}, {tr("30 Seconds"), 30}, {tr("1 Minute"), 60},
                           {tr("2 Minutes"), 120}, {tr("5 Minutes"), 300}, {tr("10 Minutes"), 600}}));
    form->addRow(tr("Real Time Clock emulation:"),
                 comboBox(QStringLiteral("GBRTCMode"),
                          {{tr("Sync to system clock"), int(GB_RTC_MODE_SYNC_TO_HOST)}, {tr("Accurate"), int(GB_RTC_MODE_ACCURATE)}}));

    m_turboCapCheckbox = new QCheckBox(tr("Cap turbo speed to:"));
    m_turboCapSlider = new QSlider(Qt::Horizontal);
    m_turboCapSlider->setRange(150, 400);
    m_turboCapSlider->setTickPosition(QSlider::TicksAbove);
    m_turboCapSlider->setTickInterval(50);
    m_turboCapLabel = new QLabel;
    m_turboCapLabel->setMinimumWidth(m_turboCapLabel->fontMetrics().horizontalAdvance(QStringLiteral("400%")));
    const double cap = Settings::instance().doubleValue(QStringLiteral("GBTurboCap"));
    m_turboCapCheckbox->setChecked(cap != 0);
    m_turboCapSlider->setValue(cap ? int(std::round(cap * 100)) : 200);
    auto updateTurboCap = [this] {
        m_turboCapSlider->setEnabled(m_turboCapCheckbox->isChecked());
        m_turboCapLabel->setText(QStringLiteral("%1%").arg(m_turboCapSlider->value()));
        Settings::instance().setValue(QStringLiteral("GBTurboCap"),
                                      m_turboCapCheckbox->isChecked() ? m_turboCapSlider->value() / 100.0 : 0.0);
    };
    connect(m_turboCapCheckbox, &QCheckBox::toggled, this, updateTurboCap);
    connect(m_turboCapSlider, &QSlider::valueChanged, this, updateTurboCap);
    m_turboCapSlider->setEnabled(m_turboCapCheckbox->isChecked());
    m_turboCapLabel->setText(QStringLiteral("%1%").arg(m_turboCapSlider->value()));
    auto *turboRow = new QHBoxLayout;
    turboRow->addWidget(m_turboCapCheckbox);
    turboRow->addWidget(m_turboCapSlider, 1);
    turboRow->addWidget(m_turboCapLabel);
    form->addRow(turboRow);
    return page;
}

void PreferencesDialog::updateBootROMsMenu()
{
    const QString folder = Settings::instance().stringValue(QStringLiteral("GBBootROMsFolder"));
    m_bootROMsButton->clear();
    m_bootROMsButton->addItem(tr("Use built-in boot ROMs"), QStringLiteral("builtin"));
    if (!folder.isEmpty() && QFileInfo(folder).isDir()) {
        m_bootROMsButton->insertSeparator(1);
        m_bootROMsButton->addItem(QFileInfo(folder).fileName(), QStringLiteral("folder"));
        m_bootROMsButton->setItemData(m_bootROMsButton->count() - 1, folder, Qt::ToolTipRole);
        m_bootROMsButton->setCurrentIndex(m_bootROMsButton->count() - 1);
    }
    m_bootROMsButton->insertSeparator(m_bootROMsButton->count());
    m_bootROMsButton->addItem(tr("Other…"), QStringLiteral("other"));
}

// MARK: - Video

QWidget *PreferencesDialog::createVideoTab()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);

    form->addRow(tr("Scaling filter:"),
                 comboBox(QStringLiteral("GBFilter"),
                          {{tr("Nearest neighbor (Pixelated)"), QStringLiteral("NearestNeighbor")},
                           {tr("Bilinear (Blurry)"), QStringLiteral("Bilinear")},
                           {tr("Smooth bilinear (Less blurry)"), QStringLiteral("SmoothBilinear")},
                           {tr("Monochrome LCD display"), QStringLiteral("MonoLCD")},
                           {tr("LCD display"), QStringLiteral("LCD")},
                           {tr("CRT display"), QStringLiteral("CRT")},
                           {tr("Flat CRT display"), QStringLiteral("FlatCRT")},
                           {tr("Scale2x"), QStringLiteral("Scale2x")},
                           {tr("Scale4x"), QStringLiteral("Scale4x")},
                           {tr("Anti-aliased Scale2x"), QStringLiteral("AAScale2x")},
                           {tr("Anti-aliased Scale4x"), QStringLiteral("AAScale4x")},
                           {tr("HQ2x"), QStringLiteral("HQ2x")},
                           {tr("OmniScale (Any factor)"), QStringLiteral("OmniScale")},
                           {tr("OmniScale Legacy"), QStringLiteral("OmniScaleLegacy")},
                           {tr("Anti-aliased OmniScale Legacy"), QStringLiteral("AAOmniScaleLegacy")}}));
    form->addRow(QString(), checkBox(tr("Apply filters to screenshots"), QStringLiteral("GBFilterScreenshots")));

    form->addRow(tr("Color correction:"),
                 comboBox(QStringLiteral("GBColorCorrection"),
                          {{tr("Disabled"), int(GB_COLOR_CORRECTION_DISABLED)},
                           {tr("Correct color curves"), int(GB_COLOR_CORRECTION_CORRECT_CURVES)},
                           {tr("Modern – Balanced"), int(GB_COLOR_CORRECTION_MODERN_BALANCED)},
                           {tr("Modern – Accurate"), int(GB_COLOR_CORRECTION_MODERN_ACCURATE)},
                           {tr("Modern – Boost contrast"), int(GB_COLOR_CORRECTION_MODERN_BOOST_CONTRAST)},
                           {tr("Reduce contrast"), int(GB_COLOR_CORRECTION_REDUCE_CONTRAST)},
                           {tr("Harsh reality (low contrast)"), int(GB_COLOR_CORRECTION_LOW_CONTRAST)}}));

    QSlider *temperature = slider(QStringLiteral("GBLightTemperature"), -256, 256, 256);
    temperature->setTickPosition(QSlider::TicksBelow);
    temperature->setTickInterval(256);
    form->addRow(tr("Ambient light temperature:"), temperature);

    form->addRow(tr("Frame blending:"),
                 comboBox(QStringLiteral("GBFrameBlendingMode"),
                          {{tr("Disabled"), int(GB_FRAME_BLENDING_MODE_DISABLED)},
                           {tr("Simple"), int(GB_FRAME_BLENDING_MODE_SIMPLE)},
                           {tr("Accurate"), int(GB_FRAME_BLENDING_MODE_ACCURATE)}}));

    m_paletteButton = new QComboBox;
    connect(m_paletteButton, &QComboBox::activated, this, &PreferencesDialog::colorPaletteChanged);
    updatePalettesMenu();
    Settings::instance().observe(this, QStringLiteral("GBThemes"), [this](const QVariant &) { updatePalettesMenu(); }, false);
    form->addRow(tr("Color palette for monochrome models:"), m_paletteButton);

    form->addRow(tr("Display border:"),
                 comboBox(QStringLiteral("GBBorderMode"), {{tr("Never"), int(GB_BORDER_NEVER)},
                                                           {tr("Super Game Boy only"), int(GB_BORDER_SGB)},
                                                           {tr("Always"), int(GB_BORDER_ALWAYS)}}));

    form->addRow(QString(), checkBox(tr("Keep aspect ratio"), QStringLiteral("GBAspectRatioUnkept"), true));
    form->addRow(QString(), checkBox(tr("Force integer scale"), QStringLiteral("GBForceIntegerScale")));
    form->addRow(QString(), checkBox(tr("On-screen display"), QStringLiteral("GBOSDEnabled")));

    auto *font = new QFontComboBox;
    font->setFontFilters(QFontComboBox::MonospacedFonts);
    auto *fontSize = new QSpinBox;
    fontSize->setRange(8, 64);
    fontSize->setSuffix(tr(" pt"));
    Settings &settings = Settings::instance();
    settings.observe(font, QStringLiteral("GBDebuggerFont"), [font](const QVariant &value) {
        QSignalBlocker blocker(font);
        font->setCurrentFont(QFont(value.toString()));
    });
    settings.observe(fontSize, QStringLiteral("GBDebuggerFontSize"), [fontSize](const QVariant &value) {
        QSignalBlocker blocker(fontSize);
        fontSize->setValue(value.toInt());
    });
    connect(font, &QFontComboBox::currentFontChanged, this, [](const QFont &selected) {
        Settings::instance().setValue(QStringLiteral("GBDebuggerFont"), selected.family());
    });
    connect(fontSize, &QSpinBox::valueChanged, this, [](int size) {
        Settings::instance().setValue(QStringLiteral("GBDebuggerFontSize"), size);
    });
    auto *fontRow = new QHBoxLayout;
    fontRow->addWidget(font, 1);
    fontRow->addWidget(fontSize);
    form->addRow(tr("Monospace font:"), fontRow);
    return page;
}

void PreferencesDialog::updatePalettesMenu()
{
    // -[GBPreferencesWindow updatePalettesMenu]
    Settings &settings = Settings::instance();
    QSignalBlocker blocker(m_paletteButton);
    m_paletteButton->clear();
    m_paletteButton->addItem(tr("Greyscale"), 0);
    m_paletteButton->addItem(tr("Lime (Game Boy)"), 1);
    m_paletteButton->addItem(tr("Olive (Pocket)"), 2);
    m_paletteButton->addItem(tr("Teal (Light)"), 3);
    QStringList names = settings.mapValue(QStringLiteral("GBThemes")).keys();
    std::sort(names.begin(), names.end(), [](const QString &a, const QString &b) {
        return QString::localeAwareCompare(a.toLower(), b.toLower()) < 0;
    });
    if (!names.isEmpty()) {
        m_paletteButton->insertSeparator(m_paletteButton->count());
    }
    for (const QString &name : names) {
        m_paletteButton->addItem(name, QStringLiteral("theme:") + name);
    }
    m_paletteButton->insertSeparator(m_paletteButton->count());
    m_paletteButton->addItem(tr("Custom…"), QStringLiteral("custom"));

    const int mode = settings.intValue(QStringLiteral("GBColorPalette"));
    if (mode >= 0) {
        m_paletteButton->setCurrentIndex(m_paletteButton->findData(mode));
    }
    else {
        m_paletteButton->setCurrentIndex(
            m_paletteButton->findData(QStringLiteral("theme:") + settings.stringValue(QStringLiteral("GBCurrentTheme"))));
    }
}

void PreferencesDialog::colorPaletteChanged(int index)
{
    Settings &settings = Settings::instance();
    const QVariant data = m_paletteButton->itemData(index);
    const QString string = data.toString();
    if (string.startsWith(QLatin1String("theme:"))) {
        settings.setValue(QStringLiteral("GBCurrentTheme"), string.mid(6));
        settings.setValue(QStringLiteral("GBColorPalette"), -1);
    }
    else if (string == QLatin1String("custom")) {
        settings.setValue(QStringLiteral("GBColorPalette"), -1);
        if (!m_paletteEditor) {
            m_paletteEditor = new PaletteEditorDialog(this);
        }
        m_paletteEditor->reload();
        m_paletteEditor->exec();
        updatePalettesMenu();
    }
    else {
        settings.setValue(QStringLiteral("GBColorPalette"), data.toInt());
    }
}

// MARK: - Audio

QWidget *PreferencesDialog::createAudioTab()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow(tr("Volume:"), slider(QStringLiteral("GBVolume"), 0, 256, 256));
    form->addRow(tr("High-pass filter:"),
                 comboBox(QStringLiteral("GBHighpassFilter"),
                          {{tr("Disabled (Keep DC offset)"), int(GB_HIGHPASS_OFF)},
                           {tr("Accurate (Emulate hardware)"), int(GB_HIGHPASS_ACCURATE)},
                           {tr("Preserve waveform"), int(GB_HIGHPASS_REMOVE_DC_OFFSET)}}));
    form->addRow(tr("Interference volume:"), slider(QStringLiteral("GBInterferenceVolume"), 0, 256, 256));
    return page;
}

// MARK: - Controls

QWidget *PreferencesDialog::createControlsTab()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);

    m_playerButton = new QComboBox;
    m_playerButton->addItems({tr("Player 1"), tr("Player 2"), tr("Player 3"), tr("Player 4")});
    auto *playerRow = new QHBoxLayout;
    playerRow->addWidget(new QLabel(tr("Control settings for")));
    playerRow->addWidget(m_playerButton);
    playerRow->addStretch();
    layout->addLayout(playerRow);

    m_controlsTable = new QTableWidget(0, 2);
    m_controlsTable->setHorizontalHeaderLabels({tr("Action"), tr("Key")});
    m_controlsTable->verticalHeader()->hide();
    m_controlsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_controlsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_controlsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_controlsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_controlsTable, &QTableWidget::cellDoubleClicked, this, [this](int row) {
        m_buttonBeingModified = row;
        m_controlsTable->setEnabled(false);
        m_playerButton->setEnabled(false);
        reloadControlsTable();
        setFocus();
    });
    layout->addWidget(new QLabel(tr("Double-click a row, then press the new key.")));
    layout->addWidget(m_controlsTable, 1);
    connect(m_playerButton, &QComboBox::currentIndexChanged, this, [this] {
        reloadControlsTable();
        refreshJoypadMenu();
    });

    auto *controllers = new QGroupBox(tr("Controllers"));
    auto *form = new QFormLayout(controllers);
    m_configureButton = new QPushButton(tr("Configure a controller"));
    m_skipButton = new QPushButton(tr("Skip"));
    m_skipButton->setEnabled(false);
    connect(m_configureButton, &QPushButton::clicked, this, [this] {
        m_configureButton->setEnabled(false);
        m_skipButton->setEnabled(true);
        m_joystickBeingConfigured.clear();
        advanceConfigurationStateMachine();
    });
    connect(m_skipButton, &QPushButton::clicked, this, &PreferencesDialog::advanceConfigurationStateMachine);
    auto *configureRow = new QHBoxLayout;
    configureRow->addWidget(m_configureButton, 1);
    configureRow->addWidget(m_skipButton);
    form->addRow(configureRow);

    m_preferredJoypadButton = new QComboBox;
    connect(m_preferredJoypadButton, &QComboBox::activated, this, [this](int index) {
        Settings &settings = Settings::instance();
        QVariantMap defaults = settings.mapValue(QStringLiteral("JoyKitDefaultControllers"));
        const QString player = QString::number(m_playerButton->currentIndex(), 16);
        const QString id = m_preferredJoypadButton->itemData(index).toString();
        if (id.isEmpty()) {
            defaults.remove(player);
        }
        else {
            defaults[player] = id;
        }
        settings.setValue(QStringLiteral("JoyKitDefaultControllers"), defaults);
    });
    form->addRow(tr("Controller for multiplayer games:"), m_preferredJoypadButton);

    form->addRow(tr("When playing motion-control games:"),
                 checkBox(tr("Prefer joysticks over motion controls"), QStringLiteral("GBMBC7JoystickOverride")));
    form->addRow(QString(), checkBox(tr("Allow mouse controls"), QStringLiteral("GBMBC7AllowMouse")));
    form->addRow(tr("Enable rumble:"),
                 comboBox(QStringLiteral("GBRumbleMode"), {{tr("Never"), int(GB_RUMBLE_DISABLED)},
                                                           {tr("For rumble-enabled Game Paks"), int(GB_RUMBLE_CARTRIDGE_ONLY)},
                                                           {tr("Always"), int(GB_RUMBLE_ALL_GAMES)}}));
    form->addRow(tr("Rumble strength:"), slider(QStringLiteral("GBRumbleStrength"), 32, 256, 256));
    form->addRow(tr("Controller “Hotkey 1” action:"), comboBox(QStringLiteral("GBJoypadHotkey1"), hotkeyActions()));
    form->addRow(tr("Controller “Hotkey 2” action:"), comboBox(QStringLiteral("GBJoypadHotkey2"), hotkeyActions()));
    form->addRow(QString(), checkBox(tr("Analog turbo and slow-motion controls"), QStringLiteral("GBAnalogControls")));
    form->addRow(QString(), checkBox(tr("Use joysticks as faux analog controls"), QStringLiteral("GBFauxAnalogInputs")));
    form->addRow(QString(), checkBox(tr("Enable controllers while in background"), QStringLiteral("GBAllowBackgroundControllers")));
    layout->addWidget(controllers);

    GamepadManager &gamepads = GamepadManager::instance();
    connect(&gamepads, &GamepadManager::controllerConnected, this, &PreferencesDialog::refreshJoypadMenu);
    connect(&gamepads, &GamepadManager::controllerDisconnected, this, &PreferencesDialog::refreshJoypadMenu);
    connect(&gamepads, &GamepadManager::rawInput, this,
            [this](const QString &uniqueId, const QString &inputId, bool pressed, double) {
                controllerInput(uniqueId, inputId, pressed);
            });
    connect(&Settings::instance(), &Settings::changed, this, [this](const QString &key) {
        if (key.startsWith(QLatin1String("GB")) && m_buttonBeingModified < 0) {
            reloadControlsTable();
        }
    });

    reloadControlsTable();
    refreshJoypadMenu();
    return page;
}

unsigned PreferencesDialog::usesForKey(int key) const
{
    unsigned uses = 0;
    Settings &settings = Settings::instance();
    for (unsigned player = 0; player < 4; player++) {
        const int count = player == 0 ? int(GBButton::KeyboardCount) : int(GBButton::PerPlayerCount);
        for (int button = 0; button < count; button++) {
            if (settings.intValue(buttonPreferenceName(GBButton(button), player)) == key) {
                uses++;
            }
        }
    }
    return uses;
}

void PreferencesDialog::reloadControlsTable()
{
    const unsigned player = unsigned(m_playerButton->currentIndex());
    const int rows = player == 0 ? int(GBButton::KeyboardCount) : int(GBButton::PerPlayerCount);
    m_controlsTable->setRowCount(rows);
    Settings &settings = Settings::instance();
    for (int row = 0; row < rows; row++) {
        m_controlsTable->setItem(row, 0, new QTableWidgetItem(buttonName(GBButton(row))));
        auto *item = new QTableWidgetItem;
        if (row == m_buttonBeingModified) {
            item->setText(tr("Select a new key…"));
        }
        else {
            const int key = settings.intValue(buttonPreferenceName(GBButton(row), player));
            item->setText(keyDisplayName(key));
            if (key && usesForKey(key) > 1) {
                QFont bold = item->font();
                bold.setBold(true);
                item->setFont(bold);
                item->setForeground(QColor::fromRgbF(0.9375, 0.25, 0.25));
            }
        }
        m_controlsTable->setItem(row, 1, item);
    }
}

void PreferencesDialog::keyPressEvent(QKeyEvent *event)
{
    if (m_buttonBeingModified < 0) {
        QDialog::keyPressEvent(event);
        return;
    }
    Settings::instance().setValue(buttonPreferenceName(GBButton(m_buttonBeingModified), unsigned(m_playerButton->currentIndex())),
                                  event->key());
    m_buttonBeingModified = -1;
    m_controlsTable->setEnabled(true);
    m_playerButton->setEnabled(true);
    reloadControlsTable();
    m_controlsTable->setFocus();
}

void PreferencesDialog::refreshJoypadMenu()
{
    const QString selected = Settings::instance()
                                 .mapValue(QStringLiteral("JoyKitDefaultControllers"))
                                 .value(QString::number(m_playerButton->currentIndex(), 16))
                                 .toString();
    m_preferredJoypadButton->clear();
    m_preferredJoypadButton->addItem(tr("None"), QString());
    bool found = false;
    for (const auto *controller : GamepadManager::instance().controllers()) {
        m_preferredJoypadButton->addItem(QStringLiteral("%1 (%2)").arg(controller->name, controller->uniqueId), controller->uniqueId);
        if (controller->uniqueId == selected) {
            m_preferredJoypadButton->setCurrentIndex(m_preferredJoypadButton->count() - 1);
            found = true;
        }
    }
    if (!selected.isEmpty() && !found) {
        // Keep showing the preferred controller while it's disconnected.
        m_preferredJoypadButton->addItem(tr("%1 (disconnected)").arg(selected), selected);
        m_preferredJoypadButton->setCurrentIndex(m_preferredJoypadButton->count() - 1);
    }
}

void PreferencesDialog::advanceConfigurationStateMachine()
{
    m_configurationState++;
    if (m_configurationState == int(GBButton::Underclock)) {
        m_configureButton->setText(tr("Press Button for Slo-Mo")); // Full name is too long :<
    }
    else if (m_configurationState < int(GBButton::TotalCount)) {
        m_configureButton->setText(tr("Press Button for %1").arg(buttonName(GBButton(m_configurationState))));
    }
    else {
        stopConfiguration();
    }
}

void PreferencesDialog::stopConfiguration()
{
    m_configurationState = -1;
    m_configureButton->setEnabled(true);
    m_skipButton->setEnabled(false);
    m_configureButton->setText(tr("Configure a controller"));
}

void PreferencesDialog::controllerInput(const QString &uniqueId, const QString &inputId, bool pressed)
{
    // Port of -[GBPreferencesWindow controller:buttonChangedState:]
    if (m_debounce.elapsed() < 250) {
        return;
    }
    m_debounce.restart();
    if (!pressed || m_configurationState < 0 || m_configurationState >= int(GBButton::TotalCount)) {
        return;
    }
    if (m_joystickBeingConfigured.isEmpty()) {
        m_joystickBeingConfigured = uniqueId;
    }
    else if (m_joystickBeingConfigured != uniqueId) {
        return;
    }
    const auto *controller = GamepadManager::instance().controller(uniqueId);
    if (!controller) {
        return;
    }
    Settings &settings = Settings::instance();
    QVariantMap instanceMappings = settings.mapValue(QStringLiteral("JoyKitInstanceMapping"));
    QVariantMap nameMappings = settings.mapValue(QStringLiteral("JoyKitNameMapping"));
    QVariantMap mapping = m_configurationState != 0 ? instanceMappings.value(uniqueId).toMap() : QVariantMap();

    const bool isAxis = inputId.startsWith(QLatin1Char('a'));
    if (m_configurationState == int(GBButton::Underclock)) {
        mapping.remove(QStringLiteral("AnalogUnderclock"));
        if (isAxis) {
            mapping[QStringLiteral("AnalogUnderclock")] = inputId;
        }
    }
    if (m_configurationState == int(GBButton::Turbo)) {
        mapping.remove(QStringLiteral("AnalogTurbo"));
        if (isAxis) {
            mapping[QStringLiteral("AnalogTurbo")] = inputId;
        }
    }
    mapping[inputId] = int(gamepadActionForButton(m_configurationState));
    instanceMappings[uniqueId] = mapping;
    nameMappings[controller->name] = mapping;
    settings.setValue(QStringLiteral("JoyKitInstanceMapping"), instanceMappings);
    settings.setValue(QStringLiteral("JoyKitNameMapping"), nameMappings);
    advanceConfigurationStateMachine();
}

void PreferencesDialog::closeEvent(QCloseEvent *event)
{
    stopConfiguration();
    m_buttonBeingModified = -1;
    m_controlsTable->setEnabled(true);
    m_playerButton->setEnabled(true);
    QDialog::closeEvent(event);
}
