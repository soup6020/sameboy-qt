#pragma once

#include "Models.h"

#include <QDateTime>
#include <QImage>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

extern "C" {
#include <Core/gb.h>
}

class AudioOutput;

struct LogChunk {
    QString text;
    GB_log_attributes_t attributes;
    bool sideView;
};

// One emulated Game Boy plus everything Document.m keeps per window: the
// emulation thread, audio buffering, debugger I/O, save paths, link cable.
class EmulatorSession : public QObject
{
    Q_OBJECT

public:
    explicit EmulatorSession(const QString &path, QObject *parent = nullptr);
    ~EmulatorSession() override;

    // All live sessions, in creation order (NSDocumentController.documents).
    static const QList<EmulatorSession *> &allSessions();

    // Loads the ROM and performs the initial reset. Returns false (with the
    // core's log output in |error|) if the ROM could not be loaded.
    bool open(QString *error);

    GB_gameboy_t *gb() const { return m_gb; }
    QString filePath() const { return m_path; }
    QString displayName() const;
    bool isCartContainer() const;
    QString romPath();
    QString savPath() const;
    QString chtPath() const;
    QString saveStatePath(unsigned slot) const;
    bool isGBS() const { return m_isGBS; }
    const GB_gbs_info_t &gbsInfo() const { return m_gbsInfo; }

    // Lifecycle
    void start();
    void stop();
    bool isRunning() const { return m_running; }
    bool isPaused() const;
    void togglePause();
    // |model| None = plain reset keeping the model, QuickReset = quick reset,
    // anything else selects (and persists) that model first.
    void reset(EmulatedModel model = EmulatedModel::None);
    void reloadROM();
    bool hotSwap(const QString &path, QString *error);
    EmulatedModel currentModel() const { return m_currentModel; }
    bool usesAutoModel() const { return m_usesAutoModel; }
    void checkForFileChanges();

    // Runs |block| between emulation steps (or inline if not running).
    void performAtomic(const std::function<void()> &block);
    // Runs |block| atomically, returning anything the core logged meanwhile.
    QString captureOutput(const std::function<void()> &block);

    // Frame buffers (always sized for the largest, bordered, resolution).
    unsigned screenWidth() const { return GB_get_screen_width(m_gb); }
    unsigned screenHeight() const { return GB_get_screen_height(m_gb); }
    const uint32_t *currentBuffer() const;
    const uint32_t *previousBuffer() const;
    GB_frame_blending_mode_t effectiveFrameBlendingMode() const;
    QImage currentFrameImage() const;

    // Called on the emulation thread for every presented frame, before the
    // buffers flip (GBView -flip equivalent; used for rapid fire/slow-mo).
    void setFrameHook(std::function<void()> hook) { m_frameHook = std::move(hook); }
    void setRewinding(bool rewinding) { m_rewindHeld = rewinding; }
    bool isRewinding() const { return m_rewindHeld; }
    void showOSD(const QString &text) { emit osdMessage(text); }

    // Audio
    void setMuted(bool muted); // Persistent (Emulation → Mute Sound)
    bool isMuted() const;
    // Temporary silence while the window is inactive; never persisted.
    void setInactiveMuted(bool muted);
    bool isAudioPlaying() const { return m_audioPlaying; }
    bool isRecordingAudio() const { return m_recordingAudio; }
    int startAudioRecording(const QString &path, GB_audio_format_t format);
    int stopAudioRecording();
    // Tap for the GBS visualizer; called on the emulation thread.
    void setSampleTap(std::function<void(const GB_sample_t &)> tap) { m_sampleTap = std::move(tap); }

    // Save states
    bool saveState(unsigned slot);
    int loadStateFile(const QString &path, bool noErrorOnNotFound);
    void loadState(unsigned slot);

    // ROM modification (memory viewer edits)
    void setROMModified();
    bool isROMModified() const { return m_romModified; }
    bool writeROM(const QString &path);

    // Debugger
    void queueDebuggerCommand(const QString &command);
    void interruptDebugInputRead();
    // Develop → Break Debugger (Document.m -interrupt:)
    void breakDebugger();
    void setSideViewCommands(const QStringList &commands);
    void refreshSideView();
    bool isDebuggerStopped() const;
    void clearPendingConsole();
    double cpuUsageSample(size_t index) const; // index 0 = oldest
    static constexpr size_t kCpuSampleCount = 0x100;

    // Accessories and link cable
    void disconnectAllAccessories();
    void connectPrinter();
    void connectWorkboy();
    void connectLinkCable(EmulatorSession *other);
    void disconnectLinkCable();
    EmulatorSession *partner() const;
    bool isSlave() const { return m_master != nullptr; }
    void setInfraredInput(bool on);

    // Game Boy Camera: |image| is converted to 128x112 luminance. A null image
    // means "no camera" (black). Safe to call from any thread.
    void setCameraImage(const QImage &image);

signals:
    void frameReady();
    void screenSizeChanged();
    void osdMessage(const QString &text);
    void consoleOutput(const QList<LogChunk> &chunks, bool clearSideView);
    void debuggerStateChanged();
    void runningChanged();
    void warning(const QString &text);
    void romModifiedChanged(bool modified);
    void fileChanged();
    void cheatsChanged();
    void gbsLoaded();
    void rumble(double amplitude);
    void printerImage(const QImage &image);
    void printerDone();
    void alarmScheduled(unsigned seconds, const QString &friendlyName);
    void alarmCancelled(const QString &path);
    void linkChanged();
    void cameraRequested();
    void errorMessage(const QString &message);

private:
    // Core callbacks (all on the emulation thread unless noted)
    static void bootRomLoadCallback(GB_gameboy_t *gb, GB_boot_rom_t type);
    static void vblankCallback(GB_gameboy_t *gb, GB_vblank_type_t type);
    static void logCallback(GB_gameboy_t *gb, const char *string, GB_log_attributes_t attributes);
    static char *inputCallback(GB_gameboy_t *gb);
    static char *asyncInputCallback(GB_gameboy_t *gb);
    static uint32_t rgbEncode(GB_gameboy_t *gb, uint8_t r, uint8_t g, uint8_t b);
    static void sampleCallback(GB_gameboy_t *gb, GB_sample_t *sample);
    static void rumbleCallback(GB_gameboy_t *gb, double amplitude);
    static void cameraRequestCallback(GB_gameboy_t *gb);
    static uint8_t cameraGetPixelCallback(GB_gameboy_t *gb, uint8_t x, uint8_t y);
    static void printImageCallback(GB_gameboy_t *gb, uint32_t *image, uint8_t height, uint8_t topMargin,
                                   uint8_t bottomMargin, uint8_t exposure);
    static void printDoneCallback(GB_gameboy_t *gb);
    static void infraredCallback(GB_gameboy_t *gb, bool on);
    static void linkBitStartCallback(GB_gameboy_t *gb, bool bit);
    static bool linkBitEndCallback(GB_gameboy_t *gb);
    static void debuggerReloadCallback(GB_gameboy_t *gb);
    static void workboySetTime(GB_gameboy_t *gb, time_t time);
    static time_t workboyGetTime(GB_gameboy_t *gb);

    void initCommon();
    GB_model_t internalModel() const;
    EmulatedModel bestModelForROM() const;
    int loadROM(QString *warnings);
    void updatePalette();
    void vblank(GB_vblank_type_t type);
    void log(const char *string, GB_log_attributes_t attributes);
    void flushConsole();
    char *debuggerInput();
    char *asyncDebuggerInput();
    void runSideViewCommands();
    void gotSample(GB_sample_t *sample);
    void renderAudio(unsigned sampleRate, unsigned frames, GB_sample_t *buffer);
    void preRun();
    void postRun();
    void run();
    void batteryTimerFired();
    void startAudio();

    GB_gameboy_t *m_gb = nullptr;
    QString m_path;
    std::thread m_thread;
    std::thread::id m_emulationThreadId;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stopping{false};

    // Atomic blocks
    std::mutex m_atomicMutex;
    std::condition_variable m_atomicCondition;
    std::function<void()> m_pendingAtomicBlock;
    std::atomic<bool> m_hasPendingAtomicBlock{false};

    // Output capture and console
    std::recursive_mutex m_consoleMutex;
    QString *m_capturedOutput = nullptr;
    QList<LogChunk> m_pendingConsole;
    bool m_logToSideView = false;
    bool m_shouldClearSideView = false;
    QTimer m_consoleTimer;

    // Debugger input
    std::mutex m_debuggerMutex;
    std::condition_variable m_debuggerCondition;
    std::deque<std::optional<QString>> m_debuggerQueue;
    std::atomic<bool> m_inSyncInput{false};
    std::optional<QString> m_debuggerCommandWhilePaused;
    QStringList m_sideViewCommands;
    std::atomic<double> m_cpuSamples[kCpuSampleCount];
    std::atomic<size_t> m_cpuSamplePosition{0};

    // Video
    std::unique_ptr<uint32_t[]> m_buffers[3];
    std::atomic<unsigned> m_currentBuffer{0};
    std::atomic<bool> m_oddFrame{false};
    std::atomic<int> m_frameBlendingMode{GB_FRAME_BLENDING_MODE_DISABLED};
    std::atomic<bool> m_borderModeChanged{false};
    std::function<void()> m_frameHook;
    std::atomic<bool> m_rewindHeld{false};
    bool m_rewind = false;
    std::atomic<int> m_borderMode{GB_BORDER_SGB};
    std::atomic<bool> m_frameSignalPending{false};
    std::atomic<bool> m_consoleFlushPending{false};
    bool m_clearSideViewOnFlush = false;

    // Audio
    std::unique_ptr<AudioOutput> m_audio;
    std::mutex m_audioClientMutex;
    std::atomic<bool> m_audioPlaying{false};
    std::atomic<bool> m_userMuted{false}; // Cached "Mute" setting (read on the emulation thread)
    std::atomic<bool> m_inactiveMuted{false};
    bool shouldPlayAudio() const;
    void applyAudioState();
    std::mutex m_audioMutex;
    std::condition_variable m_audioCondition;
    std::vector<GB_sample_t> m_audioBuffer;
    size_t m_audioBufferPosition = 0;
    size_t m_audioBufferNeeded = 0;
    std::atomic<double> m_volume{1.0};
    bool m_recordingAudio = false;
    std::function<void(const GB_sample_t &)> m_sampleTap;

    // State
    EmulatedModel m_currentModel = EmulatedModel::Auto;
    bool m_usesAutoModel = true;
    bool m_romModified = false;
    bool m_romWarningIssued = false;
    bool m_isGBS = false;
    GB_gbs_info_t m_gbsInfo{};
    QDateTime m_fileModificationTime;
    QTimer m_batteryTimer;
    bool m_dirtyBattery = false;

    // Printer
    std::vector<uint32_t> m_printerFeed;
    bool m_printerFeedActive = false;

    // Link cable (Document.m _master/_slave)
    EmulatorSession *m_master = nullptr;
    EmulatorSession *m_slave = nullptr;
    int64_t m_linkOffset = 0;
    bool m_linkCableBit = false;

    // Camera
    std::mutex m_cameraMutex;
    std::vector<uint8_t> m_cameraPixels;
};
