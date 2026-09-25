#pragma once

#include <QTimer>
#include <QWidget>

#include <vector>

extern "C" {
#include <Core/gb.h>
}

class EmulatorSession;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

// Port of Cocoa/GBCheatSearchController + CheatSearch.xib.
class CheatSearchWindow : public QWidget
{
    Q_OBJECT

public:
    explicit CheatSearchWindow(EmulatorSession *session, QWidget *parent = nullptr);

signals:
    void cheatAdded(int row);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void reset();
    void search();
    void conditionChanged();
    void addCheat();
    void reloadCurrentValues();
    uint8_t *addressForRow(int row) const;
    GB_cheat_search_data_type_t dataType() const;

    EmulatorSession *m_session;
    QComboBox *m_dataTypeButton;
    QComboBox *m_conditionTypeButton;
    QLineEdit *m_operandField;
    QLineEdit *m_conditionField;
    QLabel *m_resultsLabel;
    QTableWidget *m_table;
    QPushButton *m_addCheatButton;
    std::vector<GB_cheat_search_result_t> m_results;
    QTimer m_refreshTimer;
    bool m_updatingTable = false;
};
