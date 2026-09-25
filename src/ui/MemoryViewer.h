#pragma once

#include "MemoryModel.h"

#include <QTimer>
#include <QWidget>

class EmulatorSession;
class HexView;
class QComboBox;
class QLabel;
class QLineEdit;

// The Memory window from Document.xib (address space, bank, go to, hex view).
class MemoryViewer : public QWidget
{
    Q_OBJECT

public:
    explicit MemoryViewer(EmulatorSession *session, QWidget *parent = nullptr);

    // Verify bank sanity after a reset/model switch (Document.m -reset:).
    void sessionReset();

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void updateSpace(int index);
    void updateBank(bool ignoreErrors);
    void goTo();
    void updateStatus();
    void updateFont();
    void showError(QWidget *anchor, const QString &error);

    EmulatorSession *m_session;
    MemoryModel m_model;
    HexView *m_hexView;
    QComboBox *m_spaceButton;
    QLineEdit *m_bankInput;
    QLineEdit *m_goToInput;
    QLabel *m_status;
    uint16_t m_bankForDescription = uint16_t(-1);
    QTimer m_refreshTimer;
};
