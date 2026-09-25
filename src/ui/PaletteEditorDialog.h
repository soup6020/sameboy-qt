#pragma once

#include <QColor>
#include <QDialog>

class QCheckBox;
class QListWidget;
class QPushButton;
class QSlider;

// Port of Cocoa/GBPaletteEditorController: edits named monochrome themes
// stored in GBThemes, with .sbp import/export.
class PaletteEditorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PaletteEditorDialog(QWidget *parent = nullptr);

    void reload(); // -awakeFromNib

private:
    QStringList sortedThemeNames() const;
    void selectionChanged();
    void loadPalette();
    void savePalette();
    void updateEnabledControls();
    void updateAutoColors();
    QColor autoColorAtPosition(double position) const;
    void setColor(int index, const QColor &color);
    void pickColor(int index);
    void addTheme();
    void deleteTheme();
    void renameTheme(const QString &oldName, const QString &newName);
    void exportTheme();
    void importTheme();
    void restoreDefaults();

    QListWidget *m_themesList;
    QPushButton *m_colorWells[5];
    QColor m_colors[5];
    QCheckBox *m_disableLCDColorCheckbox;
    QCheckBox *m_manualModeCheckbox;
    QSlider *m_brightnessSlider;
    QSlider *m_hueSlider;
    QSlider *m_hueStrengthSlider;
    bool m_loading = false;
};
