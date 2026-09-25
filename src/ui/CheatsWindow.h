#pragma once

#include <QWidget>

extern "C" {
#include <Core/gb.h>
}

class EmulatorSession;
class QCheckBox;
class QLineEdit;
class QTableWidget;

// Port of Cocoa/GBCheatWindowController and the Cheats window in Document.xib.
class CheatsWindow : public QWidget
{
    Q_OBJECT

public:
    explicit CheatsWindow(EmulatorSession *session, QWidget *parent = nullptr);

    static QString addressString(const GB_cheat_t *cheat);
    static QString actionDescription(const GB_cheat_t *cheat);

    void reload();
    void selectRow(int row);

private:
    void selectionChanged();
    void updateCheat();
    void importCheat();
    void toggleEnabled(int row);
    void removeCheat(int row);

    EmulatorSession *m_session;
    QTableWidget *m_table;
    QLineEdit *m_addressField;
    QLineEdit *m_valueField;
    QCheckBox *m_oldValueCheckbox;
    QLineEdit *m_oldValueField;
    QLineEdit *m_descriptionField;
    QLineEdit *m_importCodeField;
    QLineEdit *m_importDescriptionField;
    bool m_updatingEditors = false;
};
