#pragma once

#include <QWidget>

#include <mutex>
#include <vector>

extern "C" {
#include <Core/gb.h>
}

class EmulatorSession;
class QComboBox;
class QLabel;
class QToolButton;

// Port of Cocoa/GBVisualizerView: a stereo waveform of recent samples,
// tinted with the selected monochrome palette.
class GbsVisualizer : public QWidget
{
    Q_OBJECT

public:
    explicit GbsVisualizer(QWidget *parent = nullptr);
    void addSample(const GB_sample_t &sample); // Emulation thread

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    static constexpr size_t kSampleCount = 1024;
    std::mutex m_mutex;
    std::vector<GB_sample_t> m_samples;
    size_t m_position = 0;
};

// The GBS player UI (GBS11.xib).
class GbsPlayerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GbsPlayerWidget(EmulatorSession *session, QWidget *parent = nullptr);
    ~GbsPlayerWidget() override;

    void refresh();

private:
    void changeTrack();
    void previousTrack();
    void nextTrack();
    void updatePlayButton();

    EmulatorSession *m_session;
    QLabel *m_title;
    QLabel *m_author;
    QLabel *m_copyright;
    QComboBox *m_tracks;
    QToolButton *m_playPause;
    GbsVisualizer *m_visualizer;
};
