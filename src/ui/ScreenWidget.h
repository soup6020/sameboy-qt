#pragma once

#include <QImage>
#include <QOpenGLExtraFunctions>
#include <QOpenGLWidget>
#include <QPointer>
#include <QWidget>

class EmulatorSession;
class OSDOverlay;
class ScreenWidget;

// Renders a session's frames through upstream's MasterShader.fsh + the selected
// filter (port of GBOpenGLView/GBGLShader). Lives inside a ScreenWidget.
class GLScreenRenderer : public QOpenGLWidget, protected QOpenGLExtraFunctions
{
    Q_OBJECT

public:
    explicit GLScreenRenderer(ScreenWidget *screen);
    ~GLScreenRenderer() override;

    void reloadShader()
    {
        m_needsShaderReload = true;
        update();
    }

signals:
    // Shaders could not be built; the host should switch to software rendering.
    void failed();

protected:
    void initializeGL() override;
    void paintGL() override;

private:
    GLuint buildProgram(const QString &filterName, QString *error);
    void compileShader();
    void destroyShader();
    void drawOSD(int vx, int vy, int vw, int vh);

    ScreenWidget *m_screen;
    bool m_needsShaderReload = true;
    bool m_failed = false;
    GLuint m_program = 0;
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_texture = 0;
    GLuint m_previousTexture = 0;
    GLuint m_osdProgram = 0;
    GLuint m_osdTexture = 0;
    GLint m_resolutionUniform = -1;
    GLint m_originUniform = -1;
    GLint m_textureUniform = -1;
    GLint m_previousTextureUniform = -1;
    GLint m_blendingModeUniform = -1;
};

// The emulated LCD area of a window: layout like -[GBView setFrame:] (aspect
// ratio, integer scaling), MBC7 mouse controls, cursor hiding, drag & drop and
// the OSD. Uses GLScreenRenderer, or plain QPainter if OpenGL is unavailable.
class ScreenWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ScreenWidget(QWidget *parent = nullptr);

    void setSession(EmulatorSession *session);
    EmulatorSession *session() const { return m_session; }
    OSDOverlay *osd() const { return m_osd; }
    QString filterName() const { return m_filterName; }

    QRect screenRect() const; // Screen area inside the widget, logical pixels
    QImage renderToImage(); // Filtered output at display resolution
    void setMouseHidingEnabled(bool enabled);
    void setMouseControlEnabled(bool enabled) { m_mouseControlEnabled = enabled; }
    bool usesOpenGL() const { return m_renderer != nullptr; }

signals:
    void stateFileDropped(const QString &path);
    void romFilesDropped(const QStringList &paths);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool focusNextPrevChild(bool next) override;

private:
    void useSoftwareRendering();
    void repaintScreen();
    void updateCursor();
    bool mouseControlsActive() const;

    QPointer<EmulatorSession> m_session;
    GLScreenRenderer *m_renderer = nullptr;
    OSDOverlay *m_osd;
    QString m_filterName;
    bool m_mouseHidingEnabled = false;
    bool m_mouseInside = false;
    bool m_mouseControlEnabled = true;
    bool m_aspectRatioUnkept = false;
    bool m_forceIntegerScale = false;
};
