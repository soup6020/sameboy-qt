#pragma once

#include <QAbstractScrollArea>

class MemoryModel;

// A minimal HexFiend replacement: line numbers, 16 hex bytes and ASCII per
// line, overwrite-only editing in either column.
class HexView : public QAbstractScrollArea
{
    Q_OBJECT

public:
    explicit HexView(MemoryModel *model, QWidget *parent = nullptr);

    void reload(); // Re-read visible bytes and repaint
    void setCursorOffset(size_t offset, bool ensureVisible = true);
    size_t cursorOffset() const { return m_cursor; }

signals:
    void cursorMoved(size_t offset);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    static constexpr int kBytesPerLine = 16;
    void updateMetrics();
    void updateScrollBar();
    int visibleLines() const;
    void writeByte(size_t offset, uint8_t value);

    MemoryModel *m_model;
    size_t m_cursor = 0;
    bool m_asciiColumn = false;
    bool m_highNibbleDone = false;
    int m_charWidth = 8;
    int m_lineHeight = 16;
    int m_addressWidth = 0;
    int m_hexX = 0;
    int m_asciiX = 0;
};
