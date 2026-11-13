#include "mpv/MpvObject.h"

#include <QMetaObject>
#include <QFile>
#include <QDebug>

#include <cstring>
#include <cstdio>

MpvObject *MpvObject::s_instance = nullptr;

MpvObject::MpvObject(QObject *parent)
    : QObject(parent)
{
    s_instance = this;

    m_mpv = mpv_create();
    if (!m_mpv) {
        qFatal("parvaz: could not create mpv handle");
        return;
    }

    // IMPORTANT: mpv_set_wakeup_callback must be set BEFORE mpv_initialize.
    mpv_set_wakeup_callback(m_mpv, &MpvObject::wakeupCallback, this);

    mpv_set_option_string(m_mpv, "vo", "libmpv");   // we render into our own FBO
    mpv_set_option_string(m_mpv, "hwdec", "auto-safe");
    mpv_set_option_string(m_mpv, "keep-open", "yes");
    mpv_set_option_string(m_mpv, "idle", "yes");
    mpv_set_option_string(m_mpv, "osc", "no");
    mpv_set_option_string(m_mpv, "input-default-bindings", "no");
    mpv_set_option_string(m_mpv, "input-vo-keyboard", "no");
    mpv_set_option_string(m_mpv, "terminal", "no");
    mpv_set_option_string(m_mpv, "audio-pitch-correction", "yes");

    if (mpv_initialize(m_mpv) < 0) {
        qFatal("parvaz: mpv_initialize failed");
        return;
    }

    mpv_observe_property(m_mpv, 0, "time-pos",  MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "duration",  MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "pause",     MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 0, "media-title", MPV_FORMAT_STRING);
    mpv_observe_property(m_mpv, 0, "hwdec-current", MPV_FORMAT_STRING);

    drainEvents();
}

MpvObject::~MpvObject()
{
    if (m_mpv) {
        // The render context must be freed by its owner (the renderer)
        // before we get here. See MpvFboItem.
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
    }
    s_instance = nullptr;
}

void MpvObject::wakeupCallback(void *ctx)
{
    auto *self = static_cast<MpvObject *>(ctx);
    // Coalesce: only post one queued call while one is already pending.
    if (!self->m_wakeupPending.exchange(true))
        QMetaObject::invokeMethod(self, "onWakeup", Qt::QueuedConnection);
}

void MpvObject::onWakeup()
{
    m_wakeupPending.store(false);
    drainEvents();
}

void MpvObject::drainEvents()
{
    if (!m_mpv)
        return;

    bool hasPos = false;
    bool hasDur = false;
    bool hasPause = false;
    bool hasTitle = false;
    bool hasHwdec = false;

    for (;;) {
        mpv_event *ev = mpv_wait_event(m_mpv, 0.0);
        if (!ev || ev->event_id == MPV_EVENT_NONE)
            break;

        switch (ev->event_id) {
        case MPV_EVENT_PROPERTY_CHANGE: {
            auto *prop = static_cast<mpv_event_property *>(ev->data);
            if (!prop || !prop->name)
                break;

            const QString name = QString::fromUtf8(prop->name);

            if (name == QLatin1String("time-pos") && prop->format == MPV_FORMAT_DOUBLE) {
                m_position = *static_cast<double *>(prop->data);
                hasPos = true;
            } else if (name == QLatin1String("duration") && prop->format == MPV_FORMAT_DOUBLE) {
                m_duration = *static_cast<double *>(prop->data);
                hasDur = true;
            } else if (name == QLatin1String("pause") && prop->format == MPV_FORMAT_FLAG) {
                m_paused = *static_cast<int *>(prop->data) != 0;
                hasPause = true;
            } else if (name == QLatin1String("media-title") && prop->format == MPV_FORMAT_STRING) {
                // Must copy before the next wait_event: data is not ours.
                const char *s = *static_cast<char **>(prop->data);
                m_mediaTitle = s ? QString::fromUtf8(s) : QString();
                hasTitle = true;
            } else if (name == QLatin1String("hwdec-current") && prop->format == MPV_FORMAT_STRING) {
                const char *s = *static_cast<char **>(prop->data);
                m_hwdec = s ? QString::fromUtf8(s) : QString();
                hasHwdec = true;
            }
            break;
        }
        case MPV_EVENT_FILE_LOADED:
            emit fileLoaded();
            break;
        case MPV_EVENT_END_FILE:
            m_position = 0.0;
            hasPos = true;
            break;
        default:
            break;
        }
    }

    if (hasPos)   emit positionChanged();
    if (hasDur)   emit durationChanged();
    if (hasPause) emit pausedChanged();
    if (hasTitle) emit mediaTitleChanged();
    if (hasHwdec) emit hwdecChanged();
}

void MpvObject::setPaused(bool paused)
{
    if (!m_mpv)
        return;
    mpv_set_property_string(m_mpv, "pause", paused ? "yes" : "no");
}

void MpvObject::setRenderReady()
{
    if (m_renderReady)
        return;
    m_renderReady = true;

    if (!m_pendingFile.isEmpty()) {
        const QString pending = m_pendingFile;
        m_pendingFile.clear();
        loadFile(pending);
    }
}

void MpvObject::loadFile(const QString &path)
{
    if (!m_mpv || path.isEmpty())
        return;

    // Defer until the render context owns the video output.
    if (!m_renderReady) {
        m_pendingFile = path;
        return;
    }

    const QByteArray encoded = QFile::encodeName(path);
    const char *cmd[] = {"loadfile", encoded.constData(), "replace", nullptr};
    if (mpv_command(m_mpv, cmd) < 0)
        qWarning() << "parvaz: loadfile failed for" << path;
}

void MpvObject::togglePause()
{
    setPaused(!m_paused);
}

void MpvObject::seekRelative(double seconds)
{
    if (!m_mpv)
        return;
    const QByteArray arg = QByteArray::number(seconds, 'f', 3);
    const char *cmd[] = {"seek", arg.constData(), "relative", nullptr};
    mpv_command(m_mpv, cmd);
}

void MpvObject::setSpeed(double speed)
{
    if (!m_mpv)
        return;
    double s = speed;
    mpv_set_property(m_mpv, "speed", MPV_FORMAT_DOUBLE, &s);
}

QString MpvObject::statsLine() const
{
    if (!m_mpv)
        return QString();

    auto prop = [this](const char *name) {
        char *v = mpv_get_property_string(m_mpv, name);
        const QString out = v ? QString::fromUtf8(v) : QStringLiteral("n/a");
        mpv_free(v);
        return out;
    };

    return QStringLiteral("hwdec=%1 drop=%2 decoder-drop=%3")
        .arg(prop("hwdec-current"), prop("frame-drop-count"), prop("decoder-frame-drop-count"));
}

QString MpvObject::formatTime(double seconds) const
{
    if (!(seconds >= 0.0) || seconds > 359999.0)
        return QStringLiteral("--:--");
    const int total = static_cast<int>(seconds);
    const int h = total / 3600;
    const int m = (total % 3600) / 60;
    const int s = total % 60;
    if (h > 0)
        return QStringLiteral("%1:%2:%3")
            .arg(h)
            .arg(m, 2, 10, QLatin1Char('0'))
            .arg(s, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2")
        .arg(m, 2, 10, QLatin1Char('0'))
        .arg(s, 2, 10, QLatin1Char('0'));
}
