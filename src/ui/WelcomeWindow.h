#pragma once

#include <QImage>
#include <QMainWindow>

// The idle window shown when no game is open. It displays the SDL frontend's
// background logo, recoloured with the selected monochrome palette as
// SDL/gui.c does, and offers the application-level menus.
class WelcomeWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit WelcomeWindow(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    void buildMenus();
    void updatePalette();

    QImage m_background;
};
