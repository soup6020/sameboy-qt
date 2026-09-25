#include "CameraProvider.h"
#include "EmulatorSession.h"

#ifdef SAMEBOY_QT_CAMERA
#include <QCamera>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QPermissions>
#include <QTimer>
#include <QVideoFrame>
#include <QVideoSink>
#include <QCoreApplication>
#endif

CameraProvider::CameraProvider(EmulatorSession *session, QObject *parent)
    : QObject(parent), m_session(session)
{
    connect(session, &EmulatorSession::cameraRequested, this, &CameraProvider::requestUpdate);
}

CameraProvider::~CameraProvider()
{
#ifdef SAMEBOY_QT_CAMERA
    if (m_camera) {
        m_camera->stop();
    }
#endif
}

void CameraProvider::requestUpdate()
{
    if (!m_session) {
        return;
    }
#ifdef SAMEBOY_QT_CAMERA
    if (m_failed) {
        m_session->setCameraImage({});
        return;
    }
    if (!m_camera) {
#if QT_CONFIG(permissions)
        QCameraPermission permission;
        switch (qApp->checkPermission(permission)) {
            case Qt::PermissionStatus::Undetermined:
                qApp->requestPermission(permission, this, [this] { requestUpdate(); });
                return;
            case Qt::PermissionStatus::Denied:
                m_failed = true;
                m_session->setCameraImage({});
                return;
            case Qt::PermissionStatus::Granted:
                break;
        }
#endif
        const QCameraDevice device = QMediaDevices::defaultVideoInput();
        if (device.isNull()) {
            m_failed = true;
            m_session->setCameraImage({});
            return;
        }
        m_camera = new QCamera(device, this);
        m_captureSession = new QMediaCaptureSession(this);
        m_sink = new QVideoSink(this);
        m_captureSession->setCamera(m_camera);
        m_captureSession->setVideoSink(m_sink);
        connect(m_camera, &QCamera::errorOccurred, this, [this] {
            m_failed = true;
            if (m_waitingForFrame && m_session) {
                m_waitingForFrame = false;
                m_session->setCameraImage({});
            }
        });
        connect(m_sink, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame &frame) {
            if (!m_waitingForFrame || !m_session) {
                return;
            }
            m_waitingForFrame = false;
            m_session->setCameraImage(frame.toImage());
        });
        m_camera->start();
    }
    m_waitingForFrame = true;
    // Don't stall the game forever if the camera never delivers a frame.
    QTimer::singleShot(1000, this, [this] {
        if (m_waitingForFrame && m_session) {
            m_waitingForFrame = false;
            m_session->setCameraImage({});
        }
    });
#else
    m_session->setCameraImage({});
#endif
}
