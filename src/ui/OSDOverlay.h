#pragma once

#include <QObject>
#include <QTimer>

class QPainter;
class QWidget;

// Port of Cocoa/GBOSDView: outlined white text in the bottom-left corner that
// fades out. It is painted by its host (ScreenWidget::paintGL) rather than
// being a child widget, since child widgets aren't reliably composited on top
// of a QOpenGLWidget on every platform.
class OSDOverlay : public QObject
{
    Q_OBJECT

public:
    explicit OSDOverlay(QWidget *host);

    void displayText(const QString &text);
    void setUsesSGBScale(bool usesSGBScale) { m_usesSGBScale = usesSGBScale; }
    bool isActive() const { return !m_text.isEmpty(); }
    void paint(QPainter &painter, const QRect &screenRect) const;

signals:
    void changed(); // The host should repaint

private:
    void animate();

    QWidget *m_host;
    QString m_text;
    double m_animation = 0;
    double m_opacity = 1;
    bool m_usesSGBScale = false;
    QTimer m_timer;
};
