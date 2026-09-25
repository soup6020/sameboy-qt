#include "VramViewer.h"
#include "core/EmulatorSession.h"
#include "settings/Settings.h"

#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollArea>
#include <QStyle>
#include <QTabWidget>

// MARK: - VramImageView

VramImageView::VramImageView(QSize imageSize, int scale, QWidget *parent)
    : QWidget(parent), m_image(imageSize, QImage::Format_RGBX8888), m_scale(scale)
{
    m_image.fill(Qt::white);
    setFixedSize(imageSize * scale);
    setMouseTracking(true);
}

void VramImageView::setImage(const QImage &image)
{
    m_image = image;
    update();
}

void VramImageView::setGrids(const QList<Grid> &horizontal, const QList<Grid> &vertical)
{
    m_horizontalGrids = horizontal;
    m_verticalGrids = vertical;
    update();
}

void VramImageView::setScrollRect(const QRect &rect, bool visible)
{
    m_scrollRect = rect;
    m_displayScrollRect = visible;
    update();
}

void VramImageView::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(rect(), m_image);

    // Horizontal grids are spaced along X, vertical grids along Y (GBImageView).
    for (const Grid &grid : m_horizontalGrids) {
        painter.setPen(grid.color);
        for (int x = grid.size; x < m_image.width(); x += grid.size) {
            painter.drawLine(x * m_scale, 0, x * m_scale, height());
        }
    }
    for (const Grid &grid : m_verticalGrids) {
        painter.setPen(grid.color);
        for (int y = grid.size; y < m_image.height(); y += grid.size) {
            painter.drawLine(0, y * m_scale, width(), y * m_scale);
        }
    }

    if (m_displayScrollRect) {
        // Draw the 160x144 viewport, wrapping around the 256x256 map.
        painter.setPen(QPen(QColor(255, 0, 0, 200), 2));
        const int w = m_image.width();
        const int h = m_image.height();
        for (int dx : {0, -w}) {
            for (int dy : {0, -h}) {
                const QRect r(m_scrollRect.x() + dx, m_scrollRect.y() + dy, m_scrollRect.width(), m_scrollRect.height());
                if (r.right() < 0 || r.bottom() < 0) {
                    continue;
                }
                painter.drawRect(QRect(r.topLeft() * m_scale, r.size() * m_scale).adjusted(1, 1, -1, -1));
            }
        }
    }
}

void VramImageView::mouseMoveEvent(QMouseEvent *event)
{
    const QPoint point = event->pos() / m_scale;
    if (point.x() >= 0 && point.y() >= 0 && point.x() < m_image.width() && point.y() < m_image.height()) {
        emit hovered(point.x(), point.y());
    }
}

void VramImageView::leaveEvent(QEvent *)
{
    emit left();
}

// MARK: - VramViewer

namespace {

QComboBox *paletteMenu(bool withEffective)
{
    auto *combo = new QComboBox;
    combo->addItem(QObject::tr("None"));
    if (withEffective) {
        combo->addItem(QObject::tr("Effective Palettes"));
    }
    for (int i = 0; i < 8; i++) {
        combo->addItem(QObject::tr("Background Palette %1").arg(i));
    }
    for (int i = 0; i < 8; i++) {
        combo->addItem(QObject::tr("Object Palette %1").arg(i));
    }
    return combo;
}

QImage imageFromBuffer(const uint32_t *buffer, int width, int height)
{
    return QImage(reinterpret_cast<const uchar *>(buffer), width, height, width * 4, QImage::Format_RGBX8888).copy();
}

} // namespace

VramViewer::VramViewer(EmulatorSession *session, QWidget *parent)
    : QWidget(parent, Qt::Window), m_session(session)
{
    m_gridCheckbox = new QCheckBox(tr("Grid"));
    m_scrollCheckbox = new QCheckBox(tr("Scrolling"));

    // Tileset
    m_tilesetView = new VramImageView(QSize(256, 192), 2);
    m_tilesetPalette = paletteMenu(false);
    auto *tilesetPage = new QWidget;
    auto *tilesetLayout = new QVBoxLayout(tilesetPage);
    auto *tilesetBar = new QHBoxLayout;
    tilesetBar->addWidget(m_tilesetPalette);
    tilesetBar->addStretch();
    tilesetLayout->addLayout(tilesetBar);
    tilesetLayout->addWidget(m_tilesetView, 0, Qt::AlignCenter);

    // Tilemap
    m_tilemapView = new VramImageView(QSize(256, 256), 2);
    m_tilemapPalette = paletteMenu(true);
    m_tilemapPalette->setCurrentIndex(1);
    m_tilemapMap = new QComboBox;
    m_tilemapMap->addItems({tr("Effective Tilemap"), tr("Tilemap at $9800"), tr("Tilemap at $9C00")});
    m_tilemapSet = new QComboBox;
    m_tilemapSet->addItems({tr("Effective Tileset"), tr("Tileset at $8800"), tr("Tileset at $8000")});
    auto *tilemapPage = new QWidget;
    auto *tilemapLayout = new QVBoxLayout(tilemapPage);
    auto *tilemapBar = new QHBoxLayout;
    tilemapBar->addWidget(m_tilemapPalette);
    tilemapBar->addWidget(m_tilemapMap);
    tilemapBar->addWidget(m_tilemapSet);
    tilemapBar->addWidget(m_scrollCheckbox);
    tilemapBar->addStretch();
    tilemapLayout->addLayout(tilemapBar);
    tilemapLayout->addWidget(m_tilemapView, 0, Qt::AlignCenter);

    // Objects
    m_objectsContainer = new QWidget;
    m_objectsLayout = new QGridLayout(m_objectsContainer);
    m_objectsLayout->setSpacing(0);
    for (int i = 0; i < 40; i++) {
        ObjectItem item;
        item.widget = new QFrame;
        static_cast<QFrame *>(item.widget)->setFrameShape(QFrame::StyledPanel);
        item.widget->setAutoFillBackground(true);
        item.widget->setMinimumWidth(item.widget->fontMetrics().horizontalAdvance(QStringLiteral("0000(-000, -000)---00")) + 50);
        item.widget->setBackgroundRole((i / 4) % 2 ? QPalette::AlternateBase : QPalette::Base);
        auto *grid = new QGridLayout(item.widget);
        grid->setContentsMargins(4, 4, 4, 4);
        grid->setHorizontalSpacing(6);
        grid->setVerticalSpacing(0);
        item.image = new QLabel;
        item.image->setFixedSize(34, 34);
        item.image->setAlignment(Qt::AlignCenter);
        item.oamAddress = new QLabel;
        item.oamAddress->setToolTip(tr("OAM address"));
        item.position = new QLabel;
        item.position->setToolTip(tr("Position"));
        item.attributes = new QLabel;
        item.attributes->setToolTip(tr("Attributes"));
        item.tile = new QLabel;
        item.tile->setToolTip(tr("Tile index"));
        item.tileAddress = new QLabel;
        item.tileAddress->setToolTip(tr("Tile address"));
        item.warning = new QLabel;
        item.warning->setPixmap(style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(14, 14));
        item.warning->setToolTip(tr("Dropped: too many objects in line"));
        grid->addWidget(item.image, 0, 0, 3, 1);
        grid->addWidget(item.oamAddress, 0, 1);
        grid->addWidget(item.warning, 0, 2, Qt::AlignRight);
        grid->addWidget(item.position, 1, 1);
        grid->addWidget(item.attributes, 1, 2);
        grid->addWidget(item.tile, 2, 1);
        grid->addWidget(item.tileAddress, 2, 2);
        m_objectsLayout->addWidget(item.widget, i / 4, i % 4);
        m_objectItems << item;
    }
    auto *objectsScroll = new QScrollArea;
    objectsScroll->setWidget(m_objectsContainer);
    objectsScroll->setWidgetResizable(true);

    // Palettes
    auto *palettesPage = new QWidget;
    auto *palettesLayout = new QGridLayout(palettesPage);
    palettesLayout->setSpacing(2);
    for (int row = 0; row < 16; row++) {
        auto *label = new QLabel(row < 8 ? tr("Background %1").arg(row) : tr("Object %1").arg(row % 8));
        palettesLayout->addWidget(label, row, 0);
        m_paletteLabels << label;
        for (int color = 0; color < 4; color++) {
            auto *swatch = new QLabel;
            swatch->setAlignment(Qt::AlignCenter);
            swatch->setAutoFillBackground(true);
            swatch->setMinimumSize(72, 20);
            palettesLayout->addWidget(swatch, row, color + 1);
            m_paletteSwatches << swatch;
        }
    }

    m_tabs = new QTabWidget;
    m_tabs->addTab(tilesetPage, tr("Tileset"));
    m_tabs->addTab(tilemapPage, tr("Tilemap"));
    m_tabs->addTab(objectsScroll, tr("Objects"));
    m_tabs->addTab(palettesPage, tr("Palettes"));
    m_tabs->setCornerWidget(m_gridCheckbox);

    m_status = new QLabel;
    m_status->setMinimumHeight(m_status->fontMetrics().height() + 4);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_tabs, 1);
    layout->addWidget(m_status);

    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int index) {
        m_gridCheckbox->setVisible(index < 2);
        m_status->clear();
        reload();
    });
    connect(m_gridCheckbox, &QCheckBox::toggled, this, &VramViewer::updateGrids);
    connect(m_scrollCheckbox, &QCheckBox::toggled, this, &VramViewer::reload);
    for (QComboBox *combo : {m_tilesetPalette, m_tilemapPalette, m_tilemapMap, m_tilemapSet}) {
        connect(combo, &QComboBox::currentIndexChanged, this, &VramViewer::reload);
    }
    connect(m_tilesetView, &VramImageView::hovered, this, &VramViewer::tilesetHovered);
    connect(m_tilemapView, &VramImageView::hovered, this, &VramViewer::tilemapHovered);
    connect(m_tilesetView, &VramImageView::left, m_status, &QLabel::clear);
    connect(m_tilemapView, &VramImageView::left, m_status, &QLabel::clear);

    connect(session, &EmulatorSession::frameReady, this, [this] {
        if (isVisible()) {
            reload();
        }
    });
    connect(session, &EmulatorSession::consoleOutput, this, [this] {
        if (isVisible()) {
            reload();
        }
    });
    Settings::instance().observe(this, QStringLiteral("GBDebuggerFont"), [this](const QVariant &) { reload(); }, false);
    resize(qMax(sizeHint().width(), m_objectsContainer->sizeHint().width() + 40), 384 + 140);
}

QFont VramViewer::debuggerFont(int size) const
{
    QFont font(Settings::instance().stringValue(QStringLiteral("GBDebuggerFont")));
    font.setStyleHint(QFont::Monospace);
    font.setFixedPitch(true);
    font.setPointSize(size);
    return font;
}

void VramViewer::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    reload();
}

void VramViewer::updateGrids()
{
    if (m_gridCheckbox->isChecked()) {
        const QColor light(0, 0, 0, 64), strong(0, 0, 0, 128);
        m_tilesetView->setGrids({{light, 8}, {strong, 128}}, {{light, 8}, {strong, 64}});
        m_tilemapView->setGrids({{light, 8}}, {{light, 8}});
    }
    else {
        m_tilesetView->setGrids({}, {});
        m_tilemapView->setGrids({}, {});
    }
}

void VramViewer::reload()
{
    GB_gameboy_t *gb = m_session->gb();
    if (!GB_is_inited(gb)) {
        return;
    }
    // Port of -[Document reloadVRAMData:]
    switch (m_tabs->currentIndex()) {
        case 0: {
            GB_palette_type_t paletteType = GB_PALETTE_NONE;
            const int index = m_tilesetPalette->currentIndex();
            if (index) {
                paletteType = index > 8 ? GB_PALETTE_OAM : GB_PALETTE_BACKGROUND;
            }
            std::vector<uint32_t> buffer(256 * 192);
            GB_draw_tileset(gb, buffer.data(), paletteType, uint8_t((index - 1) & 7));
            m_tilesetView->setImage(imageFromBuffer(buffer.data(), 256, 192));
            break;
        }
        case 1: {
            GB_palette_type_t paletteType = GB_PALETTE_NONE;
            const int index = m_tilemapPalette->currentIndex();
            if (index > 1) {
                paletteType = index > 9 ? GB_PALETTE_OAM : GB_PALETTE_BACKGROUND;
            }
            else if (index == 1) {
                paletteType = GB_PALETTE_AUTO;
            }
            std::vector<uint32_t> buffer(256 * 256);
            GB_draw_tilemap(gb, buffer.data(), paletteType, uint8_t((index - 2) & 7),
                            GB_map_type_t(m_tilemapMap->currentIndex()), GB_tileset_type_t(m_tilemapSet->currentIndex()));
            const auto *io = static_cast<const uint8_t *>(GB_get_direct_access(gb, GB_DIRECT_ACCESS_IO, nullptr, nullptr));
            m_tilemapView->setScrollRect(QRect(io[GB_IO_SCX], io[GB_IO_SCY], 160, 144), m_scrollCheckbox->isChecked());
            m_tilemapView->setImage(imageFromBuffer(buffer.data(), 256, 256));
            break;
        }
        case 2:
            reloadObjects();
            break;
        case 3:
            reloadPalettes();
            break;
    }
}

void VramViewer::reloadObjects()
{
    GB_gameboy_t *gb = m_session->gb();
    GB_oam_info_t info[40];
    uint8_t height = 8;
    const uint8_t count = GB_get_oam_info(gb, info, &height);
    const bool cgb = GB_is_cgb(gb);
    const QFont font = debuggerFont(11);
    QFont boldFont = font;
    boldFont.setBold(true);
    for (int i = 0; i < 40; i++) {
        ObjectItem &item = m_objectItems[i];
        if (i >= count) {
            item.widget->setVisible(false);
            continue;
        }
        item.widget->setVisible(true);
        for (QLabel *label : {item.position, item.attributes, item.tile, item.tileAddress}) {
            label->setFont(font);
        }
        item.oamAddress->setFont(boldFont);
        item.oamAddress->setText(QStringLiteral("$%1").arg(info[i].oam_addr, 4, 16, QLatin1Char('0')).toUpper());
        item.position->setText(QStringLiteral("(%1, %2)").arg(int(info[i].x) - 8).arg(int(info[i].y) - 16));
        item.tile->setText(QStringLiteral("$%1").arg(info[i].tile, 2, 16, QLatin1Char('0')).toUpper());
        item.tileAddress->setText(QStringLiteral("$%1").arg(0x8000 + info[i].tile * 0x10, 4, 16, QLatin1Char('0')).toUpper());
        item.warning->setVisible(info[i].obscured_by_line_limit);
        const uint8_t flags = info[i].flags;
        if (cgb) {
            item.attributes->setText(QStringLiteral("%1%2%3%4%5")
                                         .arg(flags & 0x80 ? 'P' : '-')
                                         .arg(flags & 0x40 ? 'Y' : '-')
                                         .arg(flags & 0x20 ? 'X' : '-')
                                         .arg(flags & 0x08 ? 1 : 0)
                                         .arg(flags & 0x07));
        }
        else {
            item.attributes->setText(QStringLiteral("%1%2%3%4")
                                         .arg(flags & 0x80 ? 'P' : '-')
                                         .arg(flags & 0x40 ? 'Y' : '-')
                                         .arg(flags & 0x20 ? 'X' : '-')
                                         .arg(flags & 0x10 ? 1 : 0));
        }
        const QImage image = imageFromBuffer(info[i].image, 8, height);
        const int scale = 32 / height;
        item.image->setPixmap(QPixmap::fromImage(image.scaled(8 * scale, height * scale, Qt::IgnoreAspectRatio, Qt::FastTransformation)));
    }
}

void VramViewer::reloadPalettes()
{
    GB_gameboy_t *gb = m_session->gb();
    const auto *bg = static_cast<const uint8_t *>(GB_get_direct_access(gb, GB_DIRECT_ACCESS_BGP, nullptr, nullptr));
    const auto *obj = static_cast<const uint8_t *>(GB_get_direct_access(gb, GB_DIRECT_ACCESS_OBP, nullptr, nullptr));
    const QFont font = debuggerFont(13);
    for (int i = 0; i < 4 * 8 * 2; i++) {
        const int index = i % (4 * 8);
        const uint8_t *palette = i >= 4 * 8 ? obj : bg;
        const uint16_t color = uint16_t((palette[(index << 1) + 1] << 8) | palette[index << 1]);
        const uint32_t native = GB_convert_rgb15(gb, color, false);
        const int r = color & 0x1F, g = (color >> 5) & 0x1F, b = (color >> 10) & 0x1F;
        QLabel *swatch = m_paletteSwatches[i];
        swatch->setFont(font);
        swatch->setText(QStringLiteral("$%1").arg(color, 4, 16, QLatin1Char('0')).toUpper());
        swatch->setToolTip(tr("Red: %1, Green: %2, Blue: %3").arg(r).arg(g).arg(b));
        QPalette pal = swatch->palette();
        pal.setColor(QPalette::Window, QColor(native & 0xFF, (native >> 8) & 0xFF, (native >> 16) & 0xFF));
        pal.setColor(QPalette::WindowText, r * 3 + g * 4 + b * 2 > 120 ? Qt::black : Qt::white);
        swatch->setPalette(pal);
    }
}

void VramViewer::tilesetHovered(int x, int y)
{
    const int bank = x >= 128 ? 1 : 0;
    x &= 127;
    const int tile = x / 8 + y / 8 * 16;
    m_status->setText(tr("Tile number $%1 at %2:$%3")
                          .arg(tile & 0xFF, 2, 16, QLatin1Char('0'))
                          .arg(bank)
                          .arg(0x8000 + tile * 0x10, 4, 16, QLatin1Char('0')));
}

void VramViewer::tilemapHovered(int x, int y)
{
    GB_gameboy_t *gb = m_session->gb();
    const uint16_t mapOffset = uint16_t(x / 8 + y / 8 * 32);
    uint16_t mapBase = 0x1800;
    const auto mapType = GB_map_type_t(m_tilemapMap->currentIndex());
    auto tilesetType = GB_tileset_type_t(m_tilemapSet->currentIndex());
    const uint8_t lcdc = static_cast<const uint8_t *>(GB_get_direct_access(gb, GB_DIRECT_ACCESS_IO, nullptr, nullptr))[GB_IO_LCDC];
    const auto *vram = static_cast<const uint8_t *>(GB_get_direct_access(gb, GB_DIRECT_ACCESS_VRAM, nullptr, nullptr));

    if (mapType == GB_MAP_9C00 || (mapType == GB_MAP_AUTO && (lcdc & GB_LCDC_BG_MAP))) {
        mapBase = 0x1C00;
    }
    if (tilesetType == GB_TILESET_AUTO) {
        tilesetType = (lcdc & GB_LCDC_TILE_SEL) ? GB_TILESET_8800 : GB_TILESET_8000;
    }
    const uint8_t tile = vram[mapBase + mapOffset];
    uint16_t tileAddress = 0;
    if (tilesetType == GB_TILESET_8000) {
        tileAddress = uint16_t(0x8000 + tile * 0x10);
    }
    else {
        tileAddress = uint16_t(0x9000 + int8_t(tile) * 0x10);
    }
    auto hex = [](unsigned value, int width) { return QStringLiteral("%1").arg(value, width, 16, QLatin1Char('0')); };
    if (GB_is_cgb(gb)) {
        const uint8_t attributes = vram[mapBase + mapOffset + 0x2000];
        m_status->setText(tr("Tile number $%1 (%2:$%3) at map address $%4 (Attributes: %5%6%7%8%9)")
                              .arg(hex(tile, 2))
                              .arg(attributes & 0x8 ? 1 : 0)
                              .arg(hex(tileAddress, 4))
                              .arg(hex(0x8000 + mapBase + mapOffset, 4))
                              .arg(attributes & 0x80 ? 'P' : '-')
                              .arg(attributes & 0x40 ? 'V' : '-')
                              .arg(attributes & 0x20 ? 'H' : '-')
                              .arg(attributes & 0x8 ? 1 : 0)
                              .arg(attributes & 0x7));
    }
    else {
        m_status->setText(tr("Tile number $%1 ($%2) at map address $%3")
                              .arg(hex(tile, 2), hex(tileAddress, 4), hex(0x8000 + mapBase + mapOffset, 4)));
    }
}
