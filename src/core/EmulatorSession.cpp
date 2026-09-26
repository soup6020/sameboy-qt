#include "EmulatorSession.h"
#include "AudioOutput.h"
#include "ResourceLocator.h"
#include "settings/PaletteThemes.h"
#include "settings/Settings.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTimer>

#include <cerrno>
#include <cstring>
#include <memory>

#ifdef Q_OS_UNIX
#include <fcntl.h>
#include <unistd.h>
#endif

// Mirrors Cocoa/Document.m. Method order roughly follows the original so the two
// can be compared side by side when porting upstream changes.

namespace {

constexpr unsigned kSampleRate = 96000;
constexpr size_t kMaxScreenPixels = 256 * 224;

QList<EmulatorSession *> &sessionList()
{
    static QList<EmulatorSession *> sessions;
    return sessions;
}

std::atomic<qint64> g_workboyTimeOffset{0};

EmulatorSession *sessionFor(GB_gameboy_t *gb)
{
    return static_cast<EmulatorSession *>(GB_get_user_data(gb));
}

bool isPathWritable(const QString &path)
{
#ifdef Q_OS_UNIX
    const QByteArray native = QFile::encodeName(path);
    if (access(native.constData(), W_OK) == 0) {
        return true;
    }
    int fd = creat(native.constData(), 0644);
    if (fd == -1) {
        return false;
    }
    close(fd);
    unlink(native.constData());
    return true;
#else
    QFileInfo info(path);
    if (info.exists()) {
        return info.isWritable();
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.close();
    file.remove();
    return true;
#endif
}

QString relativePath(const QString &path, const QString &directory)
{
    return QDir(directory).relativeFilePath(path);
}

} // namespace

const QList<EmulatorSession *> &EmulatorSession::allSessions()
{
    return sessionList();
}

EmulatorSession::EmulatorSession(const QString &path, QObject *parent)
    : QObject(parent), m_path(QFileInfo(path).absoluteFilePath())
{
    for (auto &sample : m_cpuSamples) {
        sample = 0;
    }
    for (auto &buffer : m_buffers) {
        buffer = std::make_unique<uint32_t[]>(kMaxScreenPixels);
    }
    m_gb = GB_alloc();
    m_volume = Settings::instance().doubleValue(QStringLiteral("GBVolume"));
    g_workboyTimeOffset = Settings::instance().value(QStringLiteral("GBWorkboyTimeOffset")).toLongLong();

    m_batteryTimer.setInterval(250);
    connect(&m_batteryTimer, &QTimer::timeout, this, &EmulatorSession::batteryTimerFired);

    sessionList().append(this);
}

EmulatorSession::~EmulatorSession()
{
    disconnectLinkCable();
    stop();
    if (m_recordingAudio) {
        GB_stop_audio_recording(m_gb);
    }
    sessionList().removeAll(this);
    if (GB_is_inited(m_gb)) {
        GB_free(m_gb);
    }
    GB_dealloc(m_gb);
}

QString EmulatorSession::displayName() const
{
    return QFileInfo(m_path).fileName();
}

// MARK: - Model selection

GB_model_t EmulatorSession::internalModel() const
{
    Settings &settings = Settings::instance();
    switch (m_currentModel) {
        case EmulatedModel::DMG: return GB_model_t(settings.intValue(QStringLiteral("GBDMGModel")));
        case EmulatedModel::None:
        case EmulatedModel::QuickReset:
        case EmulatedModel::Auto:
        case EmulatedModel::CGB: return GB_model_t(settings.intValue(QStringLiteral("GBCGBModel")));
        case EmulatedModel::SGB: {
            int model = settings.intValue(QStringLiteral("GBSGBModel"));
            if (model == (GB_MODEL_SGB | GB_MODEL_PAL_BIT_OLD)) {
                model = GB_MODEL_SGB_PAL;
            }
            return GB_model_t(model);
        }
        case EmulatedModel::MGB: return GB_MODEL_MGB;
        case EmulatedModel::AGB: return GB_model_t(settings.intValue(QStringLiteral("GBAGBModel")));
    }
    return GB_MODEL_CGB_E;
}

EmulatedModel EmulatorSession::bestModelForROM() const
{
    const auto *rom = static_cast<const uint8_t *>(GB_get_direct_access(m_gb, GB_DIRECT_ACCESS_ROM, nullptr, nullptr));
    if (!rom) {
        return EmulatedModel::CGB;
    }
    if (rom[0x143] & 0x80) { // Has CGB features
        return EmulatedModel::CGB;
    }
    if (rom[0x146] == 3) { // Has SGB features
        return EmulatedModel::SGB;
    }
    if (rom[0x14B] == 1) { // Nintendo-licensed (most likely has boot ROM palettes)
        return EmulatedModel::CGB;
    }
    if (rom[0x14B] == 0x33 && rom[0x144] == '0' && rom[0x145] == '1') { // Ditto
        return EmulatedModel::CGB;
    }
    return EmulatedModel::DMG;
}

// MARK: - Initialization

void EmulatorSession::updatePalette()
{
    GB_set_palette(m_gb, currentUserPalette());
}

void EmulatorSession::initCommon()
{
    GB_init(m_gb, internalModel());
    GB_set_user_data(m_gb, this);
    GB_set_boot_rom_load_callback(m_gb, bootRomLoadCallback);
    GB_set_vblank_callback(m_gb, vblankCallback);
    GB_set_enable_skipped_frame_vblank_callbacks(m_gb, true);
    GB_set_log_callback(m_gb, logCallback);
    GB_set_input_callback(m_gb, inputCallback);
    GB_set_async_input_callback(m_gb, asyncInputCallback);
    updatePalette();
    GB_set_rgb_encode_callback(m_gb, rgbEncode);
    GB_set_camera_get_pixel_callback(m_gb, cameraGetPixelCallback);
    GB_set_camera_update_request_callback(m_gb, cameraRequestCallback);
    GB_apu_set_sample_callback(m_gb, sampleCallback);
    GB_set_rumble_callback(m_gb, rumbleCallback);
    GB_set_infrared_callback(m_gb, infraredCallback);
    GB_debugger_set_reload_callback(m_gb, debuggerReloadCallback);
    GB_set_pixels_output(m_gb, m_buffers[(m_currentBuffer + 1) % 3].get());

    Settings &settings = Settings::instance();
    GB_gameboy_t *gb = m_gb;

    settings.observe(this, QStringLiteral("GBColorCorrection"), [gb](const QVariant &value) {
        GB_set_color_correction_mode(gb, GB_color_correction_mode_t(value.toInt()));
    });
    settings.observe(this, QStringLiteral("GBLightTemperature"),
                     [gb](const QVariant &value) { GB_set_light_temperature(gb, value.toDouble()); });
    settings.observe(this, QStringLiteral("GBInterferenceVolume"),
                     [gb](const QVariant &value) { GB_set_interference_volume(gb, value.toDouble()); });
    m_borderMode = settings.intValue(QStringLiteral("GBBorderMode"));
    GB_set_border_mode(m_gb, GB_border_mode_t(m_borderMode.load()));
    settings.observe(
        this, QStringLiteral("GBBorderMode"),
        [this](const QVariant &value) {
            m_borderMode = value.toInt();
            m_borderModeChanged = true;
            if (!m_running) {
                // Not running: apply now, the emulation thread won't do it for us.
                unsigned previousWidth = GB_get_screen_width(m_gb);
                GB_set_border_mode(m_gb, GB_border_mode_t(m_borderMode.load()));
                m_borderModeChanged = false;
                if (GB_get_screen_width(m_gb) != previousWidth) {
                    emit screenSizeChanged();
                }
            }
        },
        false);
    settings.observe(this, QStringLiteral("GBHighpassFilter"), [gb](const QVariant &value) {
        GB_set_highpass_filter_mode(gb, GB_highpass_mode_t(value.toInt()));
    });
    settings.observe(this, QStringLiteral("GBRewindLength"), [this](const QVariant &value) {
        const double length = value.toDouble();
        performAtomic([this, length] { GB_set_rewind_length(m_gb, length); });
    });
    settings.observe(this, QStringLiteral("GBRTCMode"),
                     [gb](const QVariant &value) { GB_set_rtc_mode(gb, GB_rtc_mode_t(value.toInt())); });
    settings.observe(this, QStringLiteral("GBRumbleMode"),
                     [gb](const QVariant &value) { GB_set_rumble_mode(gb, GB_rumble_mode_t(value.toInt())); });
    settings.observe(this, QStringLiteral("GBTurboCap"), [this](const QVariant &value) {
        if (!m_master) {
            GB_set_turbo_cap(m_gb, value.toDouble());
        }
    });
    settings.observe(this, QStringLiteral("GBFrameBlendingMode"), [this](const QVariant &value) {
        m_frameBlendingMode = value.toInt();
        emit frameReady();
    });
    settings.observe(this, QStringLiteral("GBVolume"), [this](const QVariant &value) { m_volume = value.toDouble(); });
    for (const char *key : {"GBColorPalette", "GBCurrentTheme", "GBThemes"}) {
        settings.observe(this, QString::fromLatin1(key), [this](const QVariant &) { updatePalette(); }, false);
    }

    const std::pair<const char *, EmulatedModel> revisionKeys[] = {
        {"GBDMGModel", EmulatedModel::DMG},
        {"GBSGBModel", EmulatedModel::SGB},
        {"GBCGBModel", EmulatedModel::CGB},
        {"GBAGBModel", EmulatedModel::AGB},
    };
    for (const auto &[key, model] : revisionKeys) {
        const EmulatedModel family = model;
        settings.observe(
            this, QString::fromLatin1(key),
            [this, family](const QVariant &) {
                if (m_currentModel == family) {
                    reset();
                }
            },
            false);
    }
}

bool EmulatorSession::open(QString *error)
{
    Settings &settings = Settings::instance();
    m_currentModel = EmulatedModel(settings.intValue(QStringLiteral("GBEmulatedModel")));
    m_usesAutoModel = m_currentModel == EmulatedModel::Auto;

    initCommon();
    QString warnings;
    if (loadROM(&warnings)) {
        if (error) {
            *error = warnings.isEmpty() ? tr("Could not load ROM") : warnings;
        }
        return false;
    }
    reset();
    return true;
}

// MARK: - Paths

bool EmulatorSession::isCartContainer() const
{
    return QFileInfo(m_path).suffix().compare(QLatin1String("gbcart"), Qt::CaseInsensitive) == 0;
}

static QString withSuffix(const QString &path, const QString &suffix)
{
    QFileInfo info(path);
    return info.dir().filePath(info.completeBaseName() + QLatin1Char('.') + suffix);
}

QString EmulatorSession::savPath() const
{
    if (isCartContainer()) {
        return m_path + QStringLiteral("/battery.sav");
    }
    return withSuffix(m_path, QStringLiteral("sav"));
}

QString EmulatorSession::chtPath() const
{
    if (isCartContainer()) {
        return m_path + QStringLiteral("/cheats.cht");
    }
    return withSuffix(m_path, QStringLiteral("cht"));
}

QString EmulatorSession::saveStatePath(unsigned slot) const
{
    if (isCartContainer()) {
        return m_path + QStringLiteral("/state.s%1").arg(slot);
    }
    return withSuffix(m_path, QStringLiteral("s%1").arg(slot));
}

QString EmulatorSession::romPath()
{
    if (!isCartContainer()) {
        return m_path;
    }
    // rom.gbl lists the ROM as a path relative to the container, then an absolute path.
    const QString gblPath = m_path + QStringLiteral("/rom.gbl");
    QFile gbl(gblPath);
    QStringList paths;
    if (gbl.open(QIODevice::ReadOnly)) {
        paths = QString::fromUtf8(gbl.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    }
    QString fileName;
    bool needsRebuild = false;
    for (const QString &path : paths) {
        const QString resolved = QDir(m_path).absoluteFilePath(path.trimmed());
        if (QFileInfo(resolved).isFile()) {
            if (!fileName.isEmpty() && fileName != QFileInfo(resolved).canonicalFilePath()) {
                needsRebuild = true;
                break;
            }
            fileName = QFileInfo(resolved).canonicalFilePath();
        }
        else {
            needsRebuild = true;
        }
    }
    if (!fileName.isEmpty() && needsRebuild) {
        QFile out(gblPath);
        if (out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            out.write(QStringLiteral("%1\n%2\n").arg(relativePath(fileName, m_path), fileName).toUtf8());
        }
    }
    return fileName;
}

// MARK: - ROM loading

int EmulatorSession::loadROM(QString *warningsOut)
{
    int ret = 0;
    const QString fileName = romPath();
    if (fileName.isEmpty()) {
        if (warningsOut) {
            *warningsOut = tr("Could not locate the ROM referenced by this Game Boy Cartridge");
        }
        return 1;
    }

    const QString extension = QFileInfo(fileName).suffix().toLower();
    const QString warnings = captureOutput([&] {
        const QByteArray nativeName = QFile::encodeName(fileName);
        if (!m_romModified) {
            GB_debugger_clear_symbols(m_gb);
            if (extension == QLatin1String("isx")) {
                ret = GB_load_isx(m_gb, nativeName.constData());
                if (!isCartContainer()) {
                    GB_load_battery(m_gb, QFile::encodeName(withSuffix(m_path, QStringLiteral("ram"))).constData());
                }
            }
            else if (extension == QLatin1String("gbs")) {
                ret = GB_load_gbs(m_gb, nativeName.constData(), &m_gbsInfo);
                if (!ret) {
                    m_isGBS = true;
                    GB_set_rendering_disabled(m_gb, true);
                }
            }
            else {
                ret = GB_load_rom(m_gb, nativeName.constData());
            }
        }
        if (GB_save_battery_size(m_gb)) {
            if (!isPathWritable(savPath())) {
                GB_log(m_gb, "The save path for this ROM is not writeable, progress will not be saved.\n");
            }
        }
        GB_load_battery(m_gb, QFile::encodeName(savPath()).constData());
        GB_load_cheats(m_gb, QFile::encodeName(chtPath()).constData(), true);
        GB_debugger_load_symbol_file(m_gb, QFile::encodeName(ResourceLocator::registersSymbolFile()).constData());
        GB_debugger_load_symbol_file(m_gb, QFile::encodeName(withSuffix(fileName, QStringLiteral("sym"))).constData());
    });
    QMetaObject::invokeMethod(this, &EmulatorSession::cheatsChanged, Qt::QueuedConnection);
    if (m_isGBS) {
        QMetaObject::invokeMethod(this, &EmulatorSession::gbsLoaded, Qt::QueuedConnection);
    }

    if (warningsOut) {
        *warningsOut = warnings;
    }
    if (!ret && !warnings.isEmpty() && !m_romWarningIssued) {
        m_romWarningIssued = true;
        QMetaObject::invokeMethod(this, [this, warnings] { emit warning(warnings); }, Qt::QueuedConnection);
    }
    m_fileModificationTime = QFileInfo(fileName).lastModified();
    if (m_usesAutoModel) {
        m_currentModel = bestModelForROM();
    }
    return ret;
}

void EmulatorSession::checkForFileChanges()
{
    if (!GB_is_inited(m_gb) || m_romModified) {
        return;
    }
    const QString path = romPath();
    if (!path.isEmpty() && QFileInfo(path).lastModified() != m_fileModificationTime) {
        reset();
        emit fileChanged();
    }
}

// MARK: - Lifecycle

void EmulatorSession::reset(EmulatedModel model)
{
    stop();
    const unsigned oldWidth = GB_get_screen_width(m_gb);

    if (model != EmulatedModel::None && model != EmulatedModel::QuickReset) {
        // User explicitly selected a model, save the preference
        m_currentModel = model;
        m_usesAutoModel = model == EmulatedModel::Auto;
        Settings::instance().setValue(QStringLiteral("GBEmulatedModel"), int(model));
    }

    // Reload the ROM, SAV and SYM files
    QString warnings;
    if (loadROM(&warnings)) {
        emit errorMessage(warnings.isEmpty() ? tr("Could not load ROM") : warnings);
    }

    if (model == EmulatedModel::QuickReset) {
        GB_quick_reset(m_gb);
    }
    else {
        GB_switch_model_and_reset(m_gb, internalModel());
    }

    if (oldWidth != GB_get_screen_width(m_gb)) {
        emit screenSizeChanged();
    }

    start();
    if (m_isGBS) {
        emit gbsLoaded();
    }

    char title[17];
    GB_get_rom_title(m_gb, title);
    emit osdMessage(QStringLiteral("SameBoy v%1\n%2\n%3")
                        .arg(QStringLiteral(GB_VERSION), QString::fromLatin1(title),
                             QStringLiteral("%1").arg(GB_get_rom_crc32(m_gb), 8, 16, QLatin1Char('0')).toUpper()));
}

void EmulatorSession::togglePause()
{
    if (m_master) {
        m_master->togglePause();
        return;
    }
    if (m_running) {
        stop();
    }
    else {
        start();
    }
}

bool EmulatorSession::isPaused() const
{
    const EmulatorSession *driver = m_master ? m_master : this;
    if (!driver->m_running || GB_debugger_is_stopped(m_gb)) {
        return true;
    }
    if (EmulatorSession *other = partner()) {
        return GB_debugger_is_stopped(other->m_gb);
    }
    return false;
}

bool EmulatorSession::isDebuggerStopped() const
{
    return GB_debugger_is_stopped(m_gb);
}

void EmulatorSession::start()
{
    if (m_master) {
        m_master->start();
        emit runningChanged();
        return;
    }
    if (m_running) {
        return;
    }
    if (m_thread.joinable()) {
        m_thread.join();
    }
    m_running = true;
    m_thread = std::thread(&EmulatorSession::run, this);
    m_batteryTimer.start();
    emit runningChanged();
    if (m_slave) {
        emit m_slave->runningChanged();
    }
}

void EmulatorSession::stop()
{
    if (m_master) {
        if (!m_master->m_running) {
            return;
        }
        GB_debugger_set_disabled(m_gb, true);
        if (GB_debugger_is_stopped(m_gb)) {
            interruptDebugInputRead();
        }
        m_master->stop();
        GB_debugger_set_disabled(m_gb, false);
        emit runningChanged();
        return;
    }
    if (!m_running) {
        return;
    }
    if (std::this_thread::get_id() == m_emulationThreadId) {
        // Can't join ourselves; the run loop will exit after this step.
        m_running = false;
        return;
    }
    GB_debugger_set_disabled(m_gb, true);
    if (GB_debugger_is_stopped(m_gb)) {
        interruptDebugInputRead();
    }
    if (m_slave) {
        GB_debugger_set_disabled(m_slave->m_gb, true);
        if (GB_debugger_is_stopped(m_slave->m_gb)) {
            m_slave->interruptDebugInputRead();
        }
    }
    {
        std::lock_guard lock(m_audioMutex);
        m_stopping = true;
    }
    m_audioCondition.notify_all();
    m_running = false;
    if (m_thread.joinable()) {
        m_thread.join();
    }
    m_batteryTimer.stop();
    GB_debugger_set_disabled(m_gb, false);
    if (m_slave) {
        GB_debugger_set_disabled(m_slave->m_gb, false);
        emit m_slave->runningChanged();
    }
    emit runningChanged();
}

void EmulatorSession::startAudio()
{
    std::lock_guard lock(m_audioClientMutex);
    if (!m_audio) {
        m_audio =
            std::make_unique<AudioOutput>([this](unsigned sampleRate, unsigned frames,
                                                 GB_sample_t *buffer) { renderAudio(sampleRate, frames, buffer); },
                                          kSampleRate);
    }
    m_audioPlaying = m_audio->start();
}

void EmulatorSession::preRun()
{
    m_emulationThreadId = std::this_thread::get_id();
    GB_set_pixels_output(m_gb, m_buffers[(m_currentBuffer + 1) % 3].get());
    GB_set_sample_rate(m_gb, kSampleRate);
    if (!Settings::instance().boolValue(QStringLiteral("Mute")) || m_isGBS) {
        startAudio();
    }
    // Clear pending alarms, don't play alarms while playing
    QMetaObject::invokeMethod(this, [this] { emit alarmCancelled(m_path); }, Qt::QueuedConnection);
}

void EmulatorSession::postRun()
{
    {
        std::lock_guard lock(m_audioMutex);
        m_audioBufferPosition = m_audioBufferNeeded = 0;
    }
    m_audioCondition.notify_all();
    {
        std::lock_guard lock(m_audioClientMutex);
        if (m_audio) {
            m_audio->stop();
        }
        m_audio.reset();
        m_audioPlaying = false;
    }
    GB_save_battery(m_gb, QFile::encodeName(savPath()).constData());
    GB_save_cheats(m_gb, QFile::encodeName(chtPath()).constData());

    const unsigned timeToAlarm = GB_time_to_alarm(m_gb);
    if (timeToAlarm) {
        QString friendlyName = QFileInfo(m_path).completeBaseName();
        static const QRegularExpression tags(QStringLiteral("\\([^)]+\\)|\\[[^\\]]+\\]"));
        friendlyName = friendlyName.remove(tags).trimmed();
        QMetaObject::invokeMethod(
            this, [this, timeToAlarm, friendlyName] { emit alarmScheduled(timeToAlarm, friendlyName); },
            Qt::QueuedConnection);
    }
    QMetaObject::invokeMethod(this, [this] { emit rumble(0); }, Qt::QueuedConnection);
}

static std::vector<uint64_t> multiplicationTableForFrequency(uint32_t frequency)
{
    std::vector<uint64_t> table(0x100);
    for (unsigned i = 0; i < 0x100; i++) {
        table[i] = uint64_t(i) * frequency;
    }
    return table;
}

void EmulatorSession::run()
{
    preRun();
    auto runPendingAtomicBlock = [this] {
        if (m_hasPendingAtomicBlock) [[unlikely]] {
            std::unique_lock lock(m_atomicMutex);
            if (m_pendingAtomicBlock) {
                m_pendingAtomicBlock();
                m_pendingAtomicBlock = nullptr;
            }
            m_hasPendingAtomicBlock = false;
            lock.unlock();
            m_atomicCondition.notify_all();
        }
    };

    if (EmulatorSession *slave = m_slave) {
        slave->preRun();
        const auto masterTable = multiplicationTableForFrequency(GB_get_clock_rate(m_gb));
        const auto slaveTable = multiplicationTableForFrequency(GB_get_clock_rate(slave->m_gb));
        while (m_running) {
            if (m_linkOffset <= 0) {
                m_linkOffset += int64_t(slaveTable[GB_run(m_gb) & 0xFF]);
            }
            else {
                m_linkOffset -= int64_t(masterTable[GB_run(slave->m_gb) & 0xFF]);
            }
            runPendingAtomicBlock();
        }
        slave->postRun();
    }
    else {
        while (m_running) {
            if (m_rewind) {
                m_rewind = false;
                GB_rewind_pop(m_gb);
                if (!GB_rewind_pop(m_gb)) {
                    m_rewind = m_rewindHeld;
                }
            }
            else {
                GB_run(m_gb);
            }
            runPendingAtomicBlock();
        }
    }
    postRun();
    m_stopping = false;
    runPendingAtomicBlock();
    m_emulationThreadId = {};
}

void EmulatorSession::performAtomic(const std::function<void()> &block)
{
    while (!GB_is_inited(m_gb)) {
        std::this_thread::yield();
    }
    bool isRunning = m_running && !GB_debugger_is_stopped(m_gb);
    if (m_master) {
        isRunning |= bool(m_master->m_running);
    }
    if (!isRunning) {
        block();
        return;
    }
    if (m_master) {
        m_master->performAtomic(block);
        return;
    }
    if (std::this_thread::get_id() == m_emulationThreadId) {
        block();
        return;
    }

    std::unique_lock lock(m_atomicMutex);
    m_pendingAtomicBlock = block;
    m_hasPendingAtomicBlock = true;
    while (m_pendingAtomicBlock) {
        m_atomicCondition.wait_for(lock, std::chrono::milliseconds(5));
        // If the emulation thread stopped (or entered the debugger prompt) in
        // the meantime, it will never pick the block up; run it here instead.
        if (m_pendingAtomicBlock && (!m_running || GB_debugger_is_stopped(m_gb))) {
            m_pendingAtomicBlock();
            m_pendingAtomicBlock = nullptr;
            m_hasPendingAtomicBlock = false;
        }
    }
}

QString EmulatorSession::captureOutput(const std::function<void()> &block)
{
    QString captured;
    {
        std::lock_guard lock(m_consoleMutex);
        m_capturedOutput = &captured;
    }
    performAtomic(block);
    {
        std::lock_guard lock(m_consoleMutex);
        m_capturedOutput = nullptr;
    }
    captured = captured.trimmed();
    return captured;
}

void EmulatorSession::batteryTimerFired()
{
    performAtomic([this] {
        if (m_dirtyBattery && !GB_get_battery_dirty(m_gb)) {
            GB_save_battery(m_gb, QFile::encodeName(savPath()).constData());
        }
        m_dirtyBattery = GB_get_battery_dirty(m_gb);
        GB_clear_battery_dirty(m_gb);
    });
}

void EmulatorSession::reloadROM()
{
    const bool wasRunning = m_running;
    if (wasRunning) {
        stop();
    }
    m_romModified = false;
    emit romModifiedChanged(false);
    QString warnings;
    if (loadROM(&warnings)) {
        emit errorMessage(warnings);
    }
    if (wasRunning) {
        start();
    }
}

bool EmulatorSession::hotSwap(const QString &path, QString *error)
{
    const QString absolute = QFileInfo(path).absoluteFilePath();
    for (EmulatorSession *session : allSessions()) {
        if (session != this && session->m_path == absolute) {
            if (error) {
                const QString name = QFileInfo(path).fileName();
                *error =
                    tr("‘%1’ is already open in another window. Close ‘%1’ before hot swapping it into this instance.")
                        .arg(name);
            }
            return false;
        }
    }
    const bool wasRunning = m_running;
    if (wasRunning) {
        stop();
    }
    GB_save_battery(m_gb, QFile::encodeName(savPath()).constData());
    m_path = absolute;
    QString warnings;
    const bool ok = loadROM(&warnings) == 0;
    if (!ok && error) {
        *error = warnings;
    }
    if (wasRunning) {
        start();
    }
    return ok;
}

// MARK: - Video

uint32_t EmulatorSession::rgbEncode(GB_gameboy_t *, uint8_t r, uint8_t g, uint8_t b)
{
    return (r << 0) | (g << 8) | (b << 16) | 0xFF000000u;
}

const uint32_t *EmulatorSession::currentBuffer() const
{
    return m_buffers[m_currentBuffer % 3].get();
}

const uint32_t *EmulatorSession::previousBuffer() const
{
    const unsigned count = m_frameBlendingMode ? 3 : 2;
    return m_buffers[(m_currentBuffer + 2) % count].get();
}

GB_frame_blending_mode_t EmulatorSession::effectiveFrameBlendingMode() const
{
    const auto mode = GB_frame_blending_mode_t(m_frameBlendingMode.load());
    if (mode == GB_FRAME_BLENDING_MODE_ACCURATE) {
        if (GB_is_sgb(m_gb)) {
            return GB_FRAME_BLENDING_MODE_SIMPLE;
        }
        return m_oddFrame ? GB_FRAME_BLENDING_MODE_ACCURATE_ODD : GB_FRAME_BLENDING_MODE_ACCURATE_EVEN;
    }
    return mode;
}

QImage EmulatorSession::currentFrameImage() const
{
    return QImage(reinterpret_cast<const uchar *>(currentBuffer()), int(screenWidth()), int(screenHeight()),
                  int(screenWidth() * 4), QImage::Format_RGBX8888)
        .copy();
}

void EmulatorSession::vblank(GB_vblank_type_t type)
{
    const double frameUsage = GB_debugger_get_frame_cpu_usage(m_gb);
    const size_t position = m_cpuSamplePosition.load();
    m_cpuSamples[position % kCpuSampleCount] = frameUsage;
    m_cpuSamplePosition = (position + 1) % kCpuSampleCount;
    if (type == GB_VBLANK_TYPE_SKIPPED_FRAME) {
        return;
    }

    if (type != GB_VBLANK_TYPE_REPEAT) {
        if (m_frameHook) {
            m_frameHook();
        }
        // GBViewBase -flip
        const unsigned count = m_frameBlendingMode ? 3 : 2;
        m_currentBuffer = (m_currentBuffer + 1) % count;
        m_oddFrame = GB_is_odd_frame(m_gb);

        if (m_borderModeChanged) {
            const unsigned previousWidth = GB_get_screen_width(m_gb);
            GB_set_border_mode(m_gb, GB_border_mode_t(m_borderMode.load()));
            if (GB_get_screen_width(m_gb) != previousWidth) {
                QMetaObject::invokeMethod(this, &EmulatorSession::screenSizeChanged, Qt::QueuedConnection);
            }
            m_borderModeChanged = false;
        }
        GB_set_pixels_output(m_gb, m_buffers[(m_currentBuffer + 1) % count].get());
    }

    if (!m_frameSignalPending.exchange(true)) {
        QMetaObject::invokeMethod(
            this,
            [this] {
                m_frameSignalPending = false;
                emit frameReady();
            },
            Qt::QueuedConnection);
    }

    if (m_rewindHeld) {
        m_rewind = true;
        QMetaObject::invokeMethod(this, [this] { emit osdMessage(tr("Rewinding…")); }, Qt::QueuedConnection);
    }
}

// MARK: - Audio

void EmulatorSession::gotSample(GB_sample_t *sample)
{
    if (m_sampleTap) {
        m_sampleTap(*sample);
    }
    std::unique_lock lock(m_audioMutex);
    if (m_audioPlaying) {
        if (m_audioBufferPosition == m_audioBuffer.size()) {
            if (m_audioBuffer.size() >= 0x4000) {
                m_audioBufferPosition = 0;
                return;
            }
            m_audioBuffer.resize(m_audioBuffer.empty() ? 512 : m_audioBuffer.size() + (m_audioBuffer.size() >> 2));
        }
        GB_sample_t scaled = *sample;
        const double volume = m_volume;
        if (volume != 1) {
            scaled.left = int16_t(scaled.left * volume);
            scaled.right = int16_t(scaled.right * volume);
        }
        m_audioBuffer[m_audioBufferPosition++] = scaled;
    }
    if (m_audioBufferNeeded && m_audioBufferPosition >= m_audioBufferNeeded) {
        m_audioBufferNeeded = 0;
        lock.unlock();
        m_audioCondition.notify_all();
    }
}

void EmulatorSession::renderAudio(unsigned sampleRate, unsigned frames, GB_sample_t *buffer)
{
    std::unique_lock lock(m_audioMutex);
    if (m_audioBufferPosition < frames) {
        m_audioBufferNeeded = frames;
        const auto wait = std::chrono::microseconds(uint64_t(frames - m_audioBufferPosition) * 1000000 / sampleRate);
        m_audioCondition.wait_for(lock, wait);
        m_audioBufferNeeded = 0;
    }

    if (m_stopping || GB_debugger_is_stopped(m_gb)) {
        memset(buffer, 0, frames * sizeof(*buffer));
        return;
    }

    if (m_audioBufferPosition < frames) {
        // Not enough audio. Don't consume what we have, to avoid more underflows.
        memcpy(buffer, m_audioBuffer.data(), m_audioBufferPosition * sizeof(*buffer));
        memset(buffer + m_audioBufferPosition, 0, (frames - m_audioBufferPosition) * sizeof(*buffer));
    }
    else if (m_audioBufferPosition < frames + 4800) {
        memcpy(buffer, m_audioBuffer.data(), frames * sizeof(*buffer));
        memmove(m_audioBuffer.data(), m_audioBuffer.data() + frames,
                (m_audioBufferPosition - frames) * sizeof(*buffer));
        m_audioBufferPosition -= frames;
    }
    else {
        // Too much latency: skip ahead
        memcpy(buffer, m_audioBuffer.data() + (m_audioBufferPosition - frames), frames * sizeof(*buffer));
        m_audioBufferPosition = 0;
    }
}

bool EmulatorSession::isMuted() const
{
    if (m_running || m_master) {
        return !m_audioPlaying;
    }
    return Settings::instance().boolValue(QStringLiteral("Mute"));
}

void EmulatorSession::setMuted(bool muted)
{
    if (m_running || (m_master && m_master->m_running)) {
        if (muted) {
            std::lock_guard lock(m_audioClientMutex);
            if (m_audio) {
                m_audio->stop();
            }
            m_audioPlaying = false;
        }
        else {
            startAudio();
        }
    }
    Settings::instance().setValue(QStringLiteral("Mute"), muted);
}

int EmulatorSession::startAudioRecording(const QString &path, GB_audio_format_t format)
{
    int error = 0;
    performAtomic([&] { error = GB_start_audio_recording(m_gb, QFile::encodeName(path).constData(), format); });
    if (!error) {
        m_recordingAudio = true;
        emit osdMessage(tr("Audio recording started"));
    }
    return error;
}

int EmulatorSession::stopAudioRecording()
{
    int error = 0;
    performAtomic([&] { error = GB_stop_audio_recording(m_gb); });
    m_recordingAudio = false;
    if (!error) {
        emit osdMessage(tr("Audio recording ended"));
    }
    return error;
}

// MARK: - Save states

bool EmulatorSession::saveState(unsigned slot)
{
    bool success = false;
    performAtomic([&] { success = GB_save_state(m_gb, QFile::encodeName(saveStatePath(slot)).constData()) == 0; });
    if (!success) {
        QApplication::beep();
        emit warning(tr("Failed to write save state."));
    }
    else {
        emit osdMessage(tr("State saved"));
    }
    return success;
}

int EmulatorSession::loadStateFile(const QString &path, bool noErrorOnNotFound)
{
    int result = 0;
    const QString error = captureOutput([&] { result = GB_load_state(m_gb, QFile::encodeName(path).constData()); });
    if (result == ENOENT && noErrorOnNotFound) {
        return ENOENT;
    }
    if (result) {
        QApplication::beep();
    }
    else {
        emit osdMessage(tr("State loaded"));
    }
    if (!error.isEmpty()) {
        emit warning(error);
    }
    return result;
}

void EmulatorSession::loadState(unsigned slot)
{
    const int ret = loadStateFile(saveStatePath(slot), true);
    if (ret == ENOENT && !isCartContainer()) {
        loadStateFile(withSuffix(m_path, QStringLiteral("sn%1").arg(slot)), false);
    }
}

// MARK: - ROM modification

void EmulatorSession::setROMModified()
{
    if (!m_romModified) {
        m_romModified = true;
        emit romModifiedChanged(true);
    }
}

bool EmulatorSession::writeROM(const QString &path)
{
    QString target = path;
    if (target.isEmpty()) {
        target = romPath();
    }
    size_t size = 0;
    const auto *data = static_cast<const char *>(GB_get_direct_access(m_gb, GB_DIRECT_ACCESS_ROM, &size, nullptr));
    QFile file(target);
    if (!data || !file.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
        file.write(data, qint64(size)) != qint64(size)) {
        return false;
    }
    m_romModified = false;
    emit romModifiedChanged(false);
    return true;
}

// MARK: - Console & debugger

void EmulatorSession::log(const char *string, GB_log_attributes_t attributes)
{
    std::lock_guard lock(m_consoleMutex);
    const QString text = QString::fromUtf8(string);
    if (m_capturedOutput) {
        m_capturedOutput->append(text);
        return;
    }
    if (!m_pendingConsole.isEmpty() && m_pendingConsole.last().attributes == attributes &&
        m_pendingConsole.last().sideView == m_logToSideView) {
        m_pendingConsole.last().text += text;
    }
    else {
        m_pendingConsole.append({text, attributes, m_logToSideView});
    }
    if (!m_consoleFlushPending.exchange(true)) {
        QMetaObject::invokeMethod(
            this, [this] { QTimer::singleShot(50, this, &EmulatorSession::flushConsole); }, Qt::QueuedConnection);
    }
}

void EmulatorSession::flushConsole()
{
    QList<LogChunk> chunks;
    bool clearSide = false;
    {
        std::lock_guard lock(m_consoleMutex);
        m_consoleFlushPending = false;
        chunks.swap(m_pendingConsole);
        clearSide = m_clearSideViewOnFlush;
        m_clearSideViewOnFlush = false;
    }
    if (!chunks.isEmpty() || clearSide) {
        emit consoleOutput(chunks, clearSide);
    }
}

void EmulatorSession::clearPendingConsole()
{
    std::lock_guard lock(m_consoleMutex);
    m_pendingConsole.clear();
}

void EmulatorSession::queueDebuggerCommand(const QString &command)
{
    if (!m_master && !m_running && !GB_debugger_is_stopped(m_gb)) {
        m_debuggerCommandWhilePaused = command;
        GB_debugger_break(m_gb);
        start();
        return;
    }
    if (!m_inSyncInput) {
        log(">", GB_log_attributes_t(0));
    }
    log(command.toUtf8().constData(), GB_log_attributes_t(0));
    log("\n", GB_log_attributes_t(0));
    {
        std::lock_guard lock(m_debuggerMutex);
        m_debuggerQueue.emplace_back(command);
    }
    m_debuggerCondition.notify_all();
}

void EmulatorSession::interruptDebugInputRead()
{
    std::lock_guard lock(m_debuggerMutex);
    m_debuggerQueue.emplace_back(std::nullopt);
    m_debuggerCondition.notify_all();
}

void EmulatorSession::setSideViewCommands(const QStringList &commands)
{
    std::lock_guard lock(m_consoleMutex);
    m_sideViewCommands = commands;
}

void EmulatorSession::runSideViewCommands()
{
    QStringList commands;
    {
        std::lock_guard lock(m_consoleMutex);
        commands = m_sideViewCommands;
        m_shouldClearSideView = false;
        m_clearSideViewOnFlush = true;
        m_logToSideView = true;
    }
    for (const QString &line : commands) {
        const QByteArray stripped = line.trimmed().toUtf8();
        if (stripped.isEmpty()) {
            continue;
        }
        GB_attributed_log(m_gb, GB_LOG_BOLD, "%s:\n", stripped.constData());
        char *dupped = strdup(stripped.constData());
        GB_debugger_execute_command(m_gb, dupped);
        free(dupped);
        GB_log(m_gb, "\n");
    }
    std::lock_guard lock(m_consoleMutex);
    m_logToSideView = false;
}

void EmulatorSession::refreshSideView()
{
    // Only safe while the emulation thread is parked at the debugger prompt.
    if (!GB_debugger_is_stopped(m_gb) || !m_inSyncInput) {
        return;
    }
    runSideViewCommands();
    flushConsole();
}

char *EmulatorSession::debuggerInput()
{
    bool wasPlaying = false;
    {
        std::lock_guard lock(m_audioClientMutex);
        wasPlaying = m_audioPlaying;
        if (wasPlaying && m_audio) {
            m_audio->stop();
            m_audioPlaying = false;
        }
    }
    m_audioCondition.notify_all();
    m_inSyncInput = true;
    runSideViewCommands();
    QMetaObject::invokeMethod(this, &EmulatorSession::debuggerStateChanged, Qt::QueuedConnection);
    if (EmulatorSession *other = partner()) {
        QMetaObject::invokeMethod(other, &EmulatorSession::debuggerStateChanged, Qt::QueuedConnection);
    }
    log(">", GB_log_attributes_t(0));
    if (m_debuggerCommandWhilePaused) {
        const QString command = *m_debuggerCommandWhilePaused;
        m_debuggerCommandWhilePaused.reset();
        QMetaObject::invokeMethod(this, [this, command] { queueDebuggerCommand(command); }, Qt::QueuedConnection);
    }

    std::optional<QString> input;
    {
        std::unique_lock lock(m_debuggerMutex);
        m_debuggerCondition.wait(lock, [this] { return !m_debuggerQueue.empty(); });
        input = m_debuggerQueue.front();
        m_debuggerQueue.pop_front();
    }
    m_inSyncInput = false;
    {
        std::lock_guard lock(m_consoleMutex);
        m_shouldClearSideView = true;
    }
    QMetaObject::invokeMethod(
        this,
        [this] {
            QTimer::singleShot(100, this, [this] {
                bool clear = false;
                {
                    std::lock_guard lock(m_consoleMutex);
                    clear = m_shouldClearSideView;
                    m_shouldClearSideView = false;
                }
                if (clear) {
                    emit consoleOutput({}, true);
                }
                emit debuggerStateChanged();
                if (EmulatorSession *other = partner()) {
                    emit other->debuggerStateChanged();
                }
            });
        },
        Qt::QueuedConnection);
    if (wasPlaying) {
        startAudio();
    }
    if (!input) {
        return nullptr;
    }
    return strdup(input->toUtf8().constData());
}

char *EmulatorSession::asyncDebuggerInput()
{
    std::lock_guard lock(m_debuggerMutex);
    if (m_debuggerQueue.empty()) {
        return nullptr;
    }
    std::optional<QString> input = m_debuggerQueue.front();
    m_debuggerQueue.pop_front();
    if (!input) {
        return nullptr;
    }
    return strdup(input->toUtf8().constData());
}

double EmulatorSession::cpuUsageSample(size_t index) const
{
    return m_cpuSamples[(m_cpuSamplePosition + index) % kCpuSampleCount];
}

// MARK: - Accessories

void EmulatorSession::disconnectAllAccessories()
{
    disconnectLinkCable();
    performAtomic([this] { GB_disconnect_serial(m_gb); });
    emit linkChanged();
}

void EmulatorSession::connectPrinter()
{
    disconnectLinkCable();
    performAtomic([this] { GB_connect_printer(m_gb, printImageCallback, printDoneCallback); });
    emit linkChanged();
}

void EmulatorSession::connectWorkboy()
{
    disconnectLinkCable();
    performAtomic([this] { GB_connect_workboy(m_gb, workboySetTime, workboyGetTime); });
    emit linkChanged();
}

EmulatorSession *EmulatorSession::partner() const
{
    return m_slave ? m_slave : m_master;
}

void EmulatorSession::disconnectLinkCable()
{
    EmulatorSession *other = partner();
    if (!other) {
        return;
    }
    bool wasRunning = m_running || other->m_running;
    stop();
    other->stop();
    other->m_master = nullptr;
    other->m_slave = nullptr;
    m_master = nullptr;
    m_slave = nullptr;
    GB_set_turbo_mode(m_gb, false, false);
    GB_set_turbo_mode(other->m_gb, false, false);
    const double cap = Settings::instance().doubleValue(QStringLiteral("GBTurboCap"));
    GB_set_turbo_cap(m_gb, cap);
    GB_set_turbo_cap(other->m_gb, cap);
    GB_set_serial_transfer_bit_start_callback(m_gb, nullptr);
    GB_set_serial_transfer_bit_end_callback(m_gb, nullptr);
    GB_set_serial_transfer_bit_start_callback(other->m_gb, nullptr);
    GB_set_serial_transfer_bit_end_callback(other->m_gb, nullptr);
    if (wasRunning) {
        other->start();
        start();
    }
    emit linkChanged();
    emit other->linkChanged();
}

void EmulatorSession::connectLinkCable(EmulatorSession *other)
{
    if (!other || other == this) {
        return;
    }
    disconnectAllAccessories();
    other->disconnectAllAccessories();

    const bool wasRunning = m_running;
    stop();
    other->stop();
    GB_set_turbo_mode(other->m_gb, true, true);
    m_slave = other;
    other->m_master = this;
    GB_set_turbo_cap(other->m_gb, 0);
    m_linkOffset = 0;
    GB_set_serial_transfer_bit_start_callback(m_gb, linkBitStartCallback);
    GB_set_serial_transfer_bit_start_callback(other->m_gb, linkBitStartCallback);
    GB_set_serial_transfer_bit_end_callback(m_gb, linkBitEndCallback);
    GB_set_serial_transfer_bit_end_callback(other->m_gb, linkBitEndCallback);
    if (wasRunning) {
        start();
    }
    emit linkChanged();
    emit other->linkChanged();
}

void EmulatorSession::setInfraredInput(bool on)
{
    GB_set_infrared_input(m_gb, on);
}

void EmulatorSession::setCameraImage(const QImage &image)
{
    {
        std::lock_guard lock(m_cameraMutex);
        if (image.isNull()) {
            m_cameraPixels.clear();
        }
        else {
            // Scale to cover 128x112 (Cocoa scales to 130x114), then center-crop.
            QImage gray = image.convertToFormat(QImage::Format_Grayscale8)
                              .scaled(130, 114, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            const int offsetX = (gray.width() - 128) / 2;
            const int offsetY = (gray.height() - 112) / 2;
            m_cameraPixels.resize(128 * 112);
            for (int y = 0; y < 112; y++) {
                memcpy(m_cameraPixels.data() + y * 128, gray.constScanLine(y + offsetY) + offsetX, 128);
            }
        }
    }
    GB_camera_updated(m_gb);
}

// MARK: - Core callbacks

void EmulatorSession::bootRomLoadCallback(GB_gameboy_t *gb, GB_boot_rom_t type)
{
    const char *name = nullptr;
    switch (type) {
        case GB_BOOT_ROM_DMG_0: name = "dmg0_boot"; break;
        case GB_BOOT_ROM_DMG: name = "dmg_boot"; break;
        case GB_BOOT_ROM_MGB: name = "mgb_boot"; break;
        case GB_BOOT_ROM_SGB: name = "sgb_boot"; break;
        case GB_BOOT_ROM_SGB2: name = "sgb2_boot"; break;
        case GB_BOOT_ROM_CGB_0: name = "cgb0_boot"; break;
        case GB_BOOT_ROM_CGB: name = "cgb_boot"; break;
        case GB_BOOT_ROM_CGB_E: name = "cgbE_boot"; break;
        case GB_BOOT_ROM_AGB_0: name = "agb0_boot"; break;
        case GB_BOOT_ROM_AGB: name = "agb_boot"; break;
    }
    if (!name) {
        return;
    }
    const QString path = ResourceLocator::bootROMPath(QString::fromLatin1(name));
    // These boot types are not commonly available, and they are identical from
    // an emulator perspective, so fall back to the more common variants.
    if (path.isEmpty() && type == GB_BOOT_ROM_CGB_E) {
        bootRomLoadCallback(gb, GB_BOOT_ROM_CGB);
        return;
    }
    if (path.isEmpty() && type == GB_BOOT_ROM_AGB_0) {
        bootRomLoadCallback(gb, GB_BOOT_ROM_AGB);
        return;
    }
    if (path.isEmpty() && type == GB_BOOT_ROM_DMG_0) {
        bootRomLoadCallback(gb, GB_BOOT_ROM_DMG);
        return;
    }
    GB_load_boot_rom(gb, QFile::encodeName(path).constData());
}

void EmulatorSession::vblankCallback(GB_gameboy_t *gb, GB_vblank_type_t type)
{
    sessionFor(gb)->vblank(type);
}

void EmulatorSession::logCallback(GB_gameboy_t *gb, const char *string, GB_log_attributes_t attributes)
{
    sessionFor(gb)->log(string, attributes);
}

char *EmulatorSession::inputCallback(GB_gameboy_t *gb)
{
    return sessionFor(gb)->debuggerInput();
}

char *EmulatorSession::asyncInputCallback(GB_gameboy_t *gb)
{
    return sessionFor(gb)->asyncDebuggerInput();
}

void EmulatorSession::sampleCallback(GB_gameboy_t *gb, GB_sample_t *sample)
{
    sessionFor(gb)->gotSample(sample);
}

void EmulatorSession::rumbleCallback(GB_gameboy_t *gb, double amplitude)
{
    EmulatorSession *self = sessionFor(gb);
    QMetaObject::invokeMethod(self, [self, amplitude] { emit self->rumble(amplitude); }, Qt::QueuedConnection);
}

void EmulatorSession::cameraRequestCallback(GB_gameboy_t *gb)
{
    EmulatorSession *self = sessionFor(gb);
    QMetaObject::invokeMethod(self, &EmulatorSession::cameraRequested, Qt::QueuedConnection);
}

uint8_t EmulatorSession::cameraGetPixelCallback(GB_gameboy_t *gb, uint8_t x, uint8_t y)
{
    EmulatorSession *self = sessionFor(gb);
    std::lock_guard lock(self->m_cameraMutex);
    if (self->m_cameraPixels.empty() || x >= 128 || y >= 112) {
        return 0;
    }
    return self->m_cameraPixels[y * 128 + x];
}

void EmulatorSession::printImageCallback(GB_gameboy_t *gb, uint32_t *image, uint8_t height, uint8_t topMargin,
                                         uint8_t bottomMargin, uint8_t)
{
    EmulatorSession *self = sessionFor(gb);
    const int totalHeight = topMargin + height + bottomMargin;
    QImage chunk(160, totalHeight, QImage::Format_RGBX8888);
    chunk.fill(Qt::white);
    for (int y = 0; y < height; y++) {
        memcpy(chunk.scanLine(topMargin + y), image + y * 160, 160 * sizeof(uint32_t));
    }
    QMetaObject::invokeMethod(self, [self, chunk] { emit self->printerImage(chunk); }, Qt::QueuedConnection);
}

void EmulatorSession::printDoneCallback(GB_gameboy_t *gb)
{
    EmulatorSession *self = sessionFor(gb);
    QMetaObject::invokeMethod(self, &EmulatorSession::printerDone, Qt::QueuedConnection);
}

void EmulatorSession::infraredCallback(GB_gameboy_t *gb, bool on)
{
    EmulatorSession *self = sessionFor(gb);
    if (EmulatorSession *other = self->partner()) {
        GB_set_infrared_input(other->m_gb, on);
    }
}

void EmulatorSession::linkBitStartCallback(GB_gameboy_t *gb, bool bit)
{
    sessionFor(gb)->m_linkCableBit = bit;
}

bool EmulatorSession::linkBitEndCallback(GB_gameboy_t *gb)
{
    EmulatorSession *self = sessionFor(gb);
    EmulatorSession *other = self->partner();
    if (!other) {
        return true;
    }
    const bool ret = GB_serial_get_data_bit(other->m_gb);
    GB_serial_set_data_bit(other->m_gb, self->m_linkCableBit);
    return ret;
}

void EmulatorSession::debuggerReloadCallback(GB_gameboy_t *gb)
{
    // Runs on the emulation thread while the debugger executes "reload".
    EmulatorSession *self = sessionFor(gb);
    QString warnings;
    self->m_romModified = false;
    self->loadROM(&warnings);
    GB_reset(gb);
}

void EmulatorSession::workboySetTime(GB_gameboy_t *, time_t t)
{
    const qint64 offset = qint64(time(nullptr)) - qint64(t);
    g_workboyTimeOffset = offset;
    QMetaObject::invokeMethod(
        qApp, [offset] { Settings::instance().setValue(QStringLiteral("GBWorkboyTimeOffset"), offset); },
        Qt::QueuedConnection);
}

time_t EmulatorSession::workboyGetTime(GB_gameboy_t *)
{
    return time(nullptr) - time_t(g_workboyTimeOffset.load());
}

void EmulatorSession::breakDebugger()
{
    log("^C\n", GB_log_attributes_t(0));
    GB_debugger_break(m_gb);
    start();
}
