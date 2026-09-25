#pragma once

#include <QObject>
#include <QPointer>

class EmulatorSession;
class QCamera;
class QMediaCaptureSession;
class QVideoSink;

// Supplies Game Boy Camera frames from the default system camera
// (Document.m -cameraRequestUpdate). Falls back to black without a camera.
class CameraProvider : public QObject
{
    Q_OBJECT

public:
    explicit CameraProvider(EmulatorSession *session, QObject *parent = nullptr);
    ~CameraProvider() override;

private:
    void requestUpdate();

    QPointer<EmulatorSession> m_session;
#ifdef SAMEBOY_QT_CAMERA
    QCamera *m_camera = nullptr;
    QMediaCaptureSession *m_captureSession = nullptr;
    QVideoSink *m_sink = nullptr;
    bool m_waitingForFrame = false;
    bool m_failed = false;
#endif
};
