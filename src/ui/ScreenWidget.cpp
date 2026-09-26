#include "ScreenWidget.h"
#include "OSDOverlay.h"
#include "core/EmulatorSession.h"
#include "core/ResourceLocator.h"
#include "settings/Settings.h"

#include <QDragEnterEvent>
#include <QFileInfo>
#include <QMimeData>
#include <QPainter>

#include <cmath>

namespace {

const char *const kVertexShader = R"(#version 150
in vec4 aPosition;
void main(void) {
    gl_Position = aPosition;
}
)";

// Composites the (premultiplied) OSD texture over the screen.
const char *const kOSDFragmentShader = R"(#version 150
uniform sampler2D image;
uniform vec2 origin;
uniform vec2 output_resolution;
out vec4 frag_color;
void main() {
    vec2 position = (gl_FragCoord.xy - origin) / output_resolution;
    position.y = 1.0 - position.y;
    frag_color = texture(image, position);
}
)";

GLuint compileStage(QOpenGLExtraFunctions *gl, GLenum type, const QByteArray &source, QString *error)
{
    GLuint shader = gl->glCreateShader(type);
    const char *data = source.constData();
    gl->glShaderSource(shader, 1, &data, nullptr);
    gl->glCompileShader(shader);
    GLint status = 0;
    gl->glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        char log[2048] = {};
        gl->glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        *error = QString::fromUtf8(log);
        gl->glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint linkProgram(QOpenGLExtraFunctions *gl, const QByteArray &fragmentSource, QString *error)
{
    GLuint vertex = compileStage(gl, GL_VERTEX_SHADER, kVertexShader, error);
    GLuint fragment = vertex ? compileStage(gl, GL_FRAGMENT_SHADER, fragmentSource, error) : 0;
    if (!vertex || !fragment) {
        if (vertex) {
            gl->glDeleteShader(vertex);
        }
        return 0;
    }
    GLuint program = gl->glCreateProgram();
    gl->glAttachShader(program, vertex);
    gl->glAttachShader(program, fragment);
    gl->glBindAttribLocation(program, 0, "aPosition"); // Both programs share one VAO
    gl->glLinkProgram(program);
    gl->glDeleteShader(vertex);
    gl->glDeleteShader(fragment);
    GLint status = 0;
    gl->glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (!status) {
        char log[2048] = {};
        gl->glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        *error = QString::fromUtf8(log);
        gl->glDeleteProgram(program);
        return 0;
    }
    return program;
}

QImage frameImage(EmulatorSession *session)
{
    const int width = int(session->screenWidth());
    const int height = int(session->screenHeight());
    return QImage(reinterpret_cast<const uchar *>(session->currentBuffer()), width, height, width * 4,
                  QImage::Format_RGBX8888);
}

} // namespace

// MARK: - GLScreenRenderer

GLScreenRenderer::GLScreenRenderer(ScreenWidget *screen) : QOpenGLWidget(screen), m_screen(screen)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFocusPolicy(Qt::NoFocus);
}

GLScreenRenderer::~GLScreenRenderer()
{
    if (context()) {
        makeCurrent();
        destroyShader();
        doneCurrent();
    }
}

void GLScreenRenderer::initializeGL()
{
    initializeOpenGLFunctions();
    m_needsShaderReload = true;
}

void GLScreenRenderer::destroyShader()
{
    for (GLuint *program : {&m_program, &m_osdProgram}) {
        if (*program) {
            glDeleteProgram(*program);
            *program = 0;
        }
    }
    for (GLuint *texture : {&m_texture, &m_previousTexture, &m_osdTexture}) {
        if (*texture) {
            glDeleteTextures(1, texture);
            *texture = 0;
        }
    }
    if (m_vbo) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
}

GLuint GLScreenRenderer::buildProgram(const QString &filterName, QString *error)
{
    QString master = ResourceLocator::shaderSource(QStringLiteral("MasterShader"));
    const QString filter = ResourceLocator::shaderSource(filterName);
    if (master.isEmpty() || filter.isEmpty()) {
        *error = QStringLiteral("shader source not found");
        return 0;
    }
    return linkProgram(this, master.replace(QStringLiteral("{filter}"), filter).toUtf8(), error);
}

void GLScreenRenderer::compileShader()
{
    m_needsShaderReload = false;
    destroyShader();

    QString error;
    const QString filterName = m_screen->filterName();
    m_program = buildProgram(filterName, &error);
    if (!m_program && filterName != QLatin1String("NearestNeighbor")) {
        qWarning("sameboy-qt: filter %s failed (%s); falling back to NearestNeighbor", qPrintable(filterName),
                 qPrintable(error));
        m_program = buildProgram(QStringLiteral("NearestNeighbor"), &error);
    }
    if (m_program) {
        m_osdProgram = linkProgram(this, kOSDFragmentShader, &error);
    }
    if (!m_program || !m_osdProgram) {
        qWarning("sameboy-qt: OpenGL shaders unavailable (%s); using software rendering", qPrintable(error));
        m_failed = true;
        QMetaObject::invokeMethod(this, &GLScreenRenderer::failed, Qt::QueuedConnection);
        return;
    }

    m_resolutionUniform = glGetUniformLocation(m_program, "output_resolution");
    m_originUniform = glGetUniformLocation(m_program, "origin");
    m_textureUniform = glGetUniformLocation(m_program, "image");
    m_previousTextureUniform = glGetUniformLocation(m_program, "previous_image");
    m_blendingModeUniform = glGetUniformLocation(m_program, "frame_blending_mode");

    for (GLuint *texture : {&m_texture, &m_previousTexture, &m_osdTexture}) {
        glGenTextures(1, texture);
        glBindTexture(GL_TEXTURE_2D, *texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    static const GLfloat quad[16] = {
        -1.f, -1.f, 0, 1, -1.f, +1.f, 0, 1, +1.f, -1.f, 0, 1, +1.f, +1.f, 0, 1,
    };
    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);
    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindVertexArray(0);
}

void GLScreenRenderer::paintGL()
{
    // Qt may leave these enabled on the widget's framebuffer; the OSD pass
    // draws at the same depth as the screen quad.
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    EmulatorSession *session = m_screen->session();
    if (!session || !GB_is_inited(session->gb()) || m_failed) {
        return;
    }
    if (m_needsShaderReload) {
        compileShader();
        if (m_failed) {
            return;
        }
    }

    const QRect rect = m_screen->screenRect();
    const double scale = devicePixelRatioF();
    const int width = int(session->screenWidth());
    const int height = int(session->screenHeight());

    // Viewport in device pixels, GL origin at the bottom-left.
    const int vx = int(std::round(rect.x() * scale));
    const int vy = int(std::round((this->height() - rect.y() - rect.height()) * scale));
    const int vw = int(std::round(rect.width() * scale));
    const int vh = int(std::round(rect.height() * scale));
    glViewport(vx, vy, vw, vh);

    const GB_frame_blending_mode_t blending = session->effectiveFrameBlendingMode();
    glUseProgram(m_program);
    glUniform2f(m_resolutionUniform, GLfloat(vw), GLfloat(vh));
    glUniform2f(m_originUniform, GLfloat(vx), GLfloat(vy));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, session->currentBuffer());
    glUniform1i(m_textureUniform, 0);
    glUniform1i(m_blendingModeUniform, blending);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_previousTexture);
    if (blending) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, session->previousBuffer());
    }
    else {
        static const uint32_t black = 0;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &black);
    }
    glUniform1i(m_previousTextureUniform, 1);
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    glActiveTexture(GL_TEXTURE0);
    glUseProgram(0);

    if (m_screen->osd()->isActive()) {
        drawOSD(vx, vy, vw, vh);
    }
}

void GLScreenRenderer::drawOSD(int vx, int vy, int vw, int vh)
{
    // QPainter isn't dependable on a core-profile context, so rasterize the
    // OSD on the CPU and composite it as a texture.
    QImage image(vw, vh, QImage::Format_RGBA8888_Premultiplied);
    image.fill(Qt::transparent);
    {
        QPainter painter(&image);
        const double scale = devicePixelRatioF();
        painter.scale(scale, scale);
        m_screen->osd()->paint(painter, QRect(0, 0, int(vw / scale), int(vh / scale)));
    }
    glViewport(vx, vy, vw, vh);
    glUseProgram(m_osdProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_osdTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, vw, vh, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.constBits());
    glUniform1i(glGetUniformLocation(m_osdProgram, "image"), 0);
    glUniform2f(glGetUniformLocation(m_osdProgram, "origin"), GLfloat(vx), GLfloat(vy));
    glUniform2f(glGetUniformLocation(m_osdProgram, "output_resolution"), GLfloat(vw), GLfloat(vh));
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // Premultiplied alpha
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glUseProgram(0);
}

// MARK: - ScreenWidget

ScreenWidget::ScreenWidget(QWidget *parent) : QWidget(parent), m_osd(new OSDOverlay(this))
{
    setFocusPolicy(Qt::StrongFocus);
    setAcceptDrops(true);
    setMouseTracking(true);
    setMinimumSize(160, 144);
    setAutoFillBackground(false);
    setAttribute(Qt::WA_OpaquePaintEvent);

    if (!qEnvironmentVariableIsSet("SAMEBOY_QT_SOFTWARE_RENDERER")) {
        m_renderer = new GLScreenRenderer(this);
        connect(m_renderer, &GLScreenRenderer::failed, this, &ScreenWidget::useSoftwareRendering);
    }

    Settings &settings = Settings::instance();
    settings.observe(this, QStringLiteral("GBFilter"), [this](const QVariant &value) {
        m_filterName = value.toString();
        if (m_renderer) {
            m_renderer->reloadShader();
        }
        update();
    });
    settings.observe(this, QStringLiteral("GBAspectRatioUnkept"), [this](const QVariant &value) {
        m_aspectRatioUnkept = value.toBool();
        repaintScreen();
    });
    settings.observe(this, QStringLiteral("GBForceIntegerScale"), [this](const QVariant &value) {
        m_forceIntegerScale = value.toBool();
        repaintScreen();
    });
}

void ScreenWidget::useSoftwareRendering()
{
    delete m_renderer;
    m_renderer = nullptr;
    update();
}

void ScreenWidget::repaintScreen()
{
    if (m_renderer) {
        m_renderer->update();
    }
    update();
}

void ScreenWidget::setSession(EmulatorSession *session)
{
    if (m_session) {
        disconnect(m_session, nullptr, this, nullptr);
    }
    m_session = session;
    if (!session) {
        return;
    }
    connect(session, &EmulatorSession::frameReady, this, &ScreenWidget::repaintScreen);
    connect(session, &EmulatorSession::screenSizeChanged, this, &ScreenWidget::repaintScreen);
    connect(session, &EmulatorSession::osdMessage, m_osd, &OSDOverlay::displayText);
    connect(m_osd, &OSDOverlay::changed, this, &ScreenWidget::repaintScreen);
}

QRect ScreenWidget::screenRect() const
{
    // Port of -[GBView setFrame:]
    QRectF frame(0, 0, width(), height());
    if (!m_session || !GB_is_inited(m_session->gb())) {
        return frame.toRect();
    }
    const double gbWidth = m_session->screenWidth();
    const double gbHeight = m_session->screenHeight();
    if (!m_aspectRatioUnkept) {
        const double ratio = frame.width() / frame.height();
        if (ratio >= gbWidth / gbHeight) {
            const double newWidth = std::round(frame.height() / gbHeight * gbWidth);
            frame.setX(std::floor((frame.width() - newWidth) / 2));
            frame.setWidth(newWidth);
        }
        else {
            const double newHeight = std::round(frame.width() / gbWidth * gbHeight);
            frame.setY(std::floor((frame.height() - newHeight) / 2));
            frame.setHeight(newHeight);
        }
    }
    if (m_forceIntegerScale) {
        const double factor = devicePixelRatioF();
        const double width = gbWidth / factor;
        const double height = gbHeight / factor;
        const double newWidth = std::max(width, std::floor(frame.width() / width) * width);
        const double newHeight = std::max(height, std::floor(frame.height() / height) * height);
        frame.translate(std::floor((frame.width() - newWidth) / 2), std::floor((frame.height() - newHeight) / 2));
        frame.setSize({newWidth, newHeight});
    }
    m_osd->setUsesSGBScale(gbWidth == 256);
    return frame.toRect();
}

void ScreenWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_renderer) {
        m_renderer->setGeometry(rect());
    }
}

void ScreenWidget::paintEvent(QPaintEvent *)
{
    if (m_renderer) {
        return; // The GL child covers everything
    }
    // Software rendering: plain scaling only.
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);
    if (!m_session || !GB_is_inited(m_session->gb())) {
        return;
    }
    const QRect target = screenRect();
    painter.setRenderHint(QPainter::SmoothPixmapTransform, m_filterName != QLatin1String("NearestNeighbor"));
    painter.drawImage(target, frameImage(m_session));
    m_osd->paint(painter, target);
}

QImage ScreenWidget::renderToImage()
{
    const QImage framebuffer = m_renderer ? m_renderer->grabFramebuffer() : grab().toImage();
    if (framebuffer.isNull()) {
        return {};
    }
    const QRect rect = screenRect();
    const double scale = framebuffer.devicePixelRatio();
    QImage cropped = framebuffer.copy(QRect(QPoint(int(rect.x() * scale), int(rect.y() * scale)),
                                            QSize(int(rect.width() * scale), int(rect.height() * scale))));
    cropped.setDevicePixelRatio(1);
    return cropped;
}

bool ScreenWidget::focusNextPrevChild(bool)
{
    return false; // Tab is a game control (rewind), not focus navigation
}

// MARK: - Mouse (MBC7 motion controls, cursor hiding)

bool ScreenWidget::mouseControlsActive() const
{
    return m_session && GB_is_inited(m_session->gb()) && GB_has_accelerometer(m_session->gb()) &&
        m_mouseControlEnabled && Settings::instance().boolValue(QStringLiteral("GBMBC7AllowMouse"));
}

void ScreenWidget::mousePressEvent(QMouseEvent *event)
{
    m_mouseControlEnabled = true;
    if (mouseControlsActive() && event->button() == Qt::LeftButton) {
        GB_set_key_state(m_session->gb(), GB_KEY_A, true);
    }
    QWidget::mousePressEvent(event);
}

void ScreenWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (mouseControlsActive() && event->button() == Qt::LeftButton) {
        GB_set_key_state(m_session->gb(), GB_KEY_A, false);
    }
    QWidget::mouseReleaseEvent(event);
}

void ScreenWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (mouseControlsActive()) {
        const QRect rect = screenRect();
        const QPointF point = event->position() - rect.topLeft();
        double x = point.x() / rect.width() * 2 - 1;
        double y = 1 - point.y() / rect.height() * 2; // Cocoa's y axis points up
        if (m_session->screenWidth() != 160) { // has border
            x *= 256 / 160.0;
            y *= 224 / 114.0;
        }
        GB_set_accelerometer_values(m_session->gb(), -x, y);
    }
    QWidget::mouseMoveEvent(event);
}

void ScreenWidget::enterEvent(QEnterEvent *event)
{
    m_mouseInside = true;
    updateCursor();
    QWidget::enterEvent(event);
}

void ScreenWidget::leaveEvent(QEvent *event)
{
    m_mouseInside = false;
    updateCursor();
    QWidget::leaveEvent(event);
}

void ScreenWidget::setMouseHidingEnabled(bool enabled)
{
    if (m_mouseHidingEnabled == enabled) {
        return;
    }
    m_mouseHidingEnabled = enabled;
    updateCursor();
}

void ScreenWidget::updateCursor()
{
    if (m_mouseHidingEnabled && m_mouseInside && !mouseControlsActive()) {
        setCursor(Qt::BlankCursor);
    }
    else {
        unsetCursor();
    }
}

// MARK: - Drag & drop

void ScreenWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void ScreenWidget::dropEvent(QDropEvent *event)
{
    QStringList roms;
    for (const QUrl &url : event->mimeData()->urls()) {
        const QString path = url.toLocalFile();
        if (path.isEmpty()) {
            continue;
        }
        if (GB_is_save_state(QFile::encodeName(path).constData())) {
            emit stateFileDropped(path);
        }
        else {
            roms << path;
        }
    }
    if (!roms.isEmpty()) {
        emit romFilesDropped(roms);
    }
    event->acceptProposedAction();
}
