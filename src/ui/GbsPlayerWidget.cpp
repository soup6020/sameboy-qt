#include "GbsPlayerWidget.h"
#include "core/EmulatorSession.h"
#include "settings/PaletteThemes.h"

#include <QBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QTimer>
#include <QToolButton>

GbsVisualizer::GbsVisualizer(QWidget *parent) : QWidget(parent), m_samples(kSampleCount)
{
    setMinimumSize(320, 96);
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, qOverload<>(&QWidget::update));
    timer->start(1000 / 60);
}

void GbsVisualizer::addSample(const GB_sample_t &sample)
{
    std::lock_guard lock(m_mutex);
    m_samples[m_position++] = sample;
    if (m_position == kSampleCount) {
        m_position = 0;
    }
}

void GbsVisualizer::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const GB_palette_t *palette = currentUserPalette();
    auto color = [](const GB_palette_t::GB_color_s &c) { return QColor(c.r, c.g, c.b); };
    painter.fillRect(rect(), color(palette->colors[0]));

    std::vector<GB_sample_t> samples;
    size_t position;
    {
        std::lock_guard lock(m_mutex);
        samples = m_samples;
        position = m_position;
    }
    const double middle = height() / 2.0;
    const double scale = height() / 2.0 / 32768.0 * 1.5;
    QPainterPath left, right;
    for (size_t i = 0; i < kSampleCount; i++) {
        const GB_sample_t &sample = samples[(i + position) % kSampleCount];
        const double x = width() * double(i) / double(kSampleCount - 1);
        const QPointF l(x, middle - sample.left * scale);
        const QPointF r(x, middle - sample.right * scale);
        if (i == 0) {
            left.moveTo(l);
            right.moveTo(r);
        }
        else {
            left.lineTo(l);
            right.lineTo(r);
        }
    }
    painter.setPen(QPen(color(palette->colors[2]), 1.5));
    painter.drawPath(right);
    painter.setPen(QPen(color(palette->colors[3]), 1.5));
    painter.drawPath(left);
}

GbsPlayerWidget::GbsPlayerWidget(EmulatorSession *session, QWidget *parent) : QWidget(parent), m_session(session)
{
    m_title = new QLabel;
    QFont titleFont = m_title->font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.4);
    m_title->setFont(titleFont);
    m_author = new QLabel;
    m_copyright = new QLabel;
    for (QLabel *label : {m_title, m_author, m_copyright}) {
        label->setAlignment(Qt::AlignCenter);
    }

    m_tracks = new QComboBox;
    auto *previous = new QToolButton;
    previous->setIcon(style()->standardIcon(QStyle::SP_MediaSkipBackward));
    previous->setToolTip(tr("Previous"));
    auto *next = new QToolButton;
    next->setIcon(style()->standardIcon(QStyle::SP_MediaSkipForward));
    next->setToolTip(tr("Next"));
    m_playPause = new QToolButton;
    auto *rewind = new QToolButton;
    rewind->setIcon(style()->standardIcon(QStyle::SP_MediaSeekBackward));
    rewind->setToolTip(tr("Restart Track"));

    m_visualizer = new GbsVisualizer;

    auto *controls = new QHBoxLayout;
    controls->addWidget(rewind);
    controls->addWidget(m_playPause);
    controls->addStretch();
    controls->addWidget(previous);
    controls->addWidget(m_tracks);
    controls->addWidget(next);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_title);
    layout->addWidget(m_author);
    layout->addWidget(m_visualizer, 1);
    layout->addLayout(controls);
    layout->addWidget(m_copyright);

    connect(m_tracks, &QComboBox::activated, this, &GbsPlayerWidget::changeTrack);
    connect(previous, &QToolButton::clicked, this, &GbsPlayerWidget::previousTrack);
    connect(next, &QToolButton::clicked, this, &GbsPlayerWidget::nextTrack);
    connect(rewind, &QToolButton::clicked, this, &GbsPlayerWidget::changeTrack);
    connect(m_playPause, &QToolButton::clicked, this, [this] { m_session->togglePause(); });
    connect(session, &EmulatorSession::runningChanged, this, &GbsPlayerWidget::updatePlayButton);

    GbsVisualizer *visualizer = m_visualizer;
    session->setSampleTap([visualizer](const GB_sample_t &sample) { visualizer->addSample(sample); });
    refresh();
    setFixedSize(sizeHint().expandedTo(QSize(420, 260)));
}

GbsPlayerWidget::~GbsPlayerWidget()
{
    m_session->setSampleTap(nullptr);
}

void GbsPlayerWidget::refresh()
{
    const GB_gbs_info_t &info = m_session->gbsInfo();
    auto latin1 = [](const char *text, const QString &fallback) {
        const QString string = QString::fromLatin1(text);
        return string.isEmpty() ? fallback : string;
    };
    m_title->setText(latin1(info.title, tr("GBS Player")));
    m_author->setText(latin1(info.author, tr("Unknown Composer")));
    const QString copyright = QString::fromLatin1(info.copyright);
    m_copyright->setText(copyright.isEmpty() ? tr("Missing copyright information") : QStringLiteral("©") + copyright);
    window()->setWindowTitle(m_title->text());

    const int selected = m_tracks->count() ? m_tracks->currentIndex() : info.first_track;
    m_tracks->clear();
    for (unsigned i = 0; i < info.track_count; i++) {
        m_tracks->addItem(tr("Track %1").arg(i + 1));
    }
    m_tracks->setCurrentIndex(qBound(0, selected, qMax(0, int(info.track_count) - 1)));
    changeTrack();
    updatePlayButton();
}

void GbsPlayerWidget::changeTrack()
{
    if (!m_session->isRunning()) {
        m_session->start();
    }
    const auto track = uint8_t(m_tracks->currentIndex());
    m_session->performAtomic([this, track] { GB_gbs_switch_track(m_session->gb(), track); });
}

void GbsPlayerWidget::previousTrack()
{
    const int count = m_tracks->count();
    if (!count) {
        return;
    }
    m_tracks->setCurrentIndex(m_tracks->currentIndex() == 0 ? count - 1 : m_tracks->currentIndex() - 1);
    changeTrack();
}

void GbsPlayerWidget::nextTrack()
{
    const int count = m_tracks->count();
    if (!count) {
        return;
    }
    m_tracks->setCurrentIndex(m_tracks->currentIndex() == count - 1 ? 0 : m_tracks->currentIndex() + 1);
    changeTrack();
}

void GbsPlayerWidget::updatePlayButton()
{
    const bool playing = m_session->isRunning();
    m_playPause->setIcon(style()->standardIcon(playing ? QStyle::SP_MediaPause : QStyle::SP_MediaPlay));
    m_playPause->setToolTip(playing ? tr("Pause") : tr("Play"));
}
