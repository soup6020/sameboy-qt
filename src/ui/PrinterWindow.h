#pragma once

#include <QImage>
#include <QWidget>

class EmulatorSession;
class QLabel;
class QProgressBar;
class QScrollArea;

// The Printer window: accumulates Game Boy Printer output as a paper feed.
class PrinterWindow : public QWidget
{
    Q_OBJECT

public:
    explicit PrinterWindow(EmulatorSession *session, QWidget *parent = nullptr);

    void appendImage(const QImage &chunk);
    void printingDone();

private:
    void save();
    void print();

    EmulatorSession *m_session;
    QImage m_feed;
    QLabel *m_imageLabel;
    QScrollArea *m_scroll;
    QProgressBar *m_spinner;
};
