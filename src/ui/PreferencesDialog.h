#pragma once

#include <QDialog>
#include <QElapsedTimer>
#include <QPointer>

class QAbstractButton;
class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSlider;
class QSpinBox;
class QTableWidget;
class QTabWidget;
class PaletteEditorDialog;

// The Preferences window (Preferences.xib + GBPreferencesWindow.m): Emulation,
// Video, Audio and Controls tabs, all bound live to Settings.
class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    enum Tab {
        EmulationTab,
        VideoTab,
        AudioTab,
        ControlsTab
    };

    explicit PreferencesDialog(QWidget *parent = nullptr);
    void setCurrentTab(Tab tab);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    QWidget *createEmulationTab();
    QWidget *createVideoTab();
    QWidget *createAudioTab();
    QWidget *createControlsTab();

    // Settings bindings
    QCheckBox *checkBox(const QString &title, const QString &key, bool invert = false);
    QComboBox *comboBox(const QString &key, const QList<std::pair<QString, QVariant>> &items,
                        const QList<QVariant> &disabled = {});
    QSlider *slider(const QString &key, int minimum, int maximum, double denominator);

    // Palette menu
    void updatePalettesMenu();
    void colorPaletteChanged(int index);

    // Boot ROMs
    void updateBootROMsMenu();

    // Controls
    void reloadControlsTable();
    void refreshControllerMenu(bool preferPlayersController);
    QString selectedControllerId() const;
    QString selectedControllerName() const;
    bool isCellBindable(int row, int column) const;
    void beginCapture(int row, int column);
    void cancelCapture();
    void clearBinding(int row, int column);
    unsigned usesForKey(int key) const;
    void refreshJoypadMenu();
    void advanceConfigurationStateMachine();
    void controllerInput(const QString &uniqueId, const QString &inputId, bool pressed);
    void stopConfiguration();

    QTabWidget *m_tabs = nullptr;
    QComboBox *m_paletteButton = nullptr;
    QPointer<PaletteEditorDialog> m_paletteEditor;
    QComboBox *m_bootROMsButton = nullptr;
    QCheckBox *m_turboCapCheckbox = nullptr;
    QSlider *m_turboCapSlider = nullptr;
    QLabel *m_turboCapLabel = nullptr;

    QComboBox *m_playerButton = nullptr;
    QTableWidget *m_controlsTable = nullptr;
    QComboBox *m_controllerButton = nullptr;
    QPushButton *m_resetControllerButton = nullptr;
    int m_captureRow = -1; // Cell waiting for a key / controller input
    int m_captureColumn = -1; // 1 = keyboard, 2 = controller
    QPushButton *m_configureButton = nullptr;
    QPushButton *m_skipButton = nullptr;
    QComboBox *m_preferredJoypadButton = nullptr;
    int m_configurationState = -1;
    QString m_joystickBeingConfigured;
    QElapsedTimer m_debounce;
};
