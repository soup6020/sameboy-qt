#pragma once

#include <QWidget>

class EmulatorSession;

// Port of Cocoa/GBCPUView: a filled line graph of per-frame CPU usage,
// green normally and red where the CPU was fully busy.
class CpuGraph : public QWidget
{
    Q_OBJECT

public:
    explicit CpuGraph(EmulatorSession *session, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    EmulatorSession *m_session;
};
