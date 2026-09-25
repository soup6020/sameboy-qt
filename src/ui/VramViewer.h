#pragma once

#include <QImage>
#include <QWidget>

extern "C" {
#include <Core/gb.h>
}

class EmulatorSession;
class QCheckBox;
class QComboBox;
class QGridLayout;
class QLabel;
class QScrollArea;
class QTabWidget;

// Port of Cocoa/GBImageView: a nearest-neighbour image view with optional
// grids and a (wrapping) scroll-viewport rectangle; reports hovered pixels.
class VramImageView : public QWidget
{
    Q_OBJECT

public:
    struct Grid {
        QColor color;
        int size;
    };

    explicit VramImageView(QSize imageSize, int scale, QWidget *parent = nullptr);
    void setImage(const QImage &image);
    void setGrids(const QList<Grid> &horizontal, const QList<Grid> &vertical);
    void setScrollRect(const QRect &rect, bool visible);

signals:
    void hovered(int x, int y);
    void left();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QImage m_image;
    int m_scale;
    QList<Grid> m_horizontalGrids, m_verticalGrids;
    QRect m_scrollRect;
    bool m_displayScrollRect = false;
};

class VramViewer : public QWidget
{
    Q_OBJECT

public:
    explicit VramViewer(EmulatorSession *session, QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *event) override;

private:
    void reload();
    void reloadObjects();
    void reloadPalettes();
    void updateGrids();
    void tilesetHovered(int x, int y);
    void tilemapHovered(int x, int y);
    QFont debuggerFont(int size) const;

    EmulatorSession *m_session;
    QTabWidget *m_tabs;
    QLabel *m_status;

    VramImageView *m_tilesetView;
    QComboBox *m_tilesetPalette;
    VramImageView *m_tilemapView;
    QComboBox *m_tilemapPalette;
    QComboBox *m_tilemapMap;
    QComboBox *m_tilemapSet;
    QCheckBox *m_gridCheckbox;
    QCheckBox *m_scrollCheckbox;

    QWidget *m_objectsContainer;
    QGridLayout *m_objectsLayout;
    struct ObjectItem {
        QWidget *widget;
        QLabel *image;
        QLabel *oamAddress;
        QLabel *position;
        QLabel *attributes;
        QLabel *tile;
        QLabel *tileAddress;
        QLabel *warning;
    };
    QList<ObjectItem> m_objectItems;
    QList<QLabel *> m_paletteSwatches;
    QList<QLabel *> m_paletteLabels;
};
