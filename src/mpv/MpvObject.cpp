#include "mpv/MpvObject.h"

#include <QMetaObject>
#include <QFile>
#include <QDebug>
#include <QVariantMap>

#include <cstring>
#include <cstdio>

MpvObject *MpvObject::s_instance = nullptr;

namespace {

/// Copies one mpv_node into a QVariant. Only the scalar formats we need are
/// handled; mpv gives us no other shape for track-list fields.
QVariant nodeToVariant(const mpv_node &node)
{
    switch (node.format) {
    case MPV_FORMAT_STRING:
        return node.u.string ? QString::fromUtf8(node.u.string) : QString();
    case MPV_FORMAT_INT64:
        return QVariant::fromValue<qlonglong>(node.u.int64);
    case MPV_FORMAT_DOUBLE:
        return node.u.double_;
    case MPV_FORMAT_FLAG:
        return node.u.flag != 0;
    default:
        return QVariant();
    }
}

} // namespace

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
    mpv_observe_property(m_mpv, 0, "volume",    MPV_FORMAT_INT64);
    mpv_observe_property(m_mpv, 0, "mute",      MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 0, "speed",     MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "media-title", MPV_FORMAT_STRING);
    mpv_observe_property(m_mpv, 0, "hwdec-current", MPV_FORMAT_STRING);
    // "track-list" is a node property: observing it with MPV_FORMAT_NONE keeps
    // mpv from serialising the whole list on every wake-up; the value itself is
    // fetched on demand in refreshTracks().
    mpv_observe_property(m_mpv, 0, "track-list", MPV_FORMAT_NONE);
    mpv_observe_property(m_mpv, 0, "sub-delay",      MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "sub-visibility", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 0, "sub-pos",        MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "sub-font-size",  MPV_FORMAT_DOUBLE);

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
    bool hasVolume = false;
    bool hasMute = false;
    bool hasSpeed = false;
    bool hasTitle = false;
    bool hasHwdec = false;
    bool hasTracks = false;
    bool hasSubDelay = false;
    bool hasSubVisible = false;
    bool hasSubPos = false;
    bool hasSubFontSize = false;

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
            } else if (name == QLatin1String("volume") && prop->format == MPV_FORMAT_INT64) {
                m_volume = *static_cast<int64_t *>(prop->data);
                hasVolume = true;
            } else if (name == QLatin1String("mute") && prop->format == MPV_FORMAT_FLAG) {
                m_muted = *static_cast<int *>(prop->data) != 0;
                hasMute = true;
            } else if (name == QLatin1String("speed") && prop->format == MPV_FORMAT_DOUBLE) {
                m_speed = *static_cast<double *>(prop->data);
                hasSpeed = true;
            } else if (name == QLatin1String("media-title") && prop->format == MPV_FORMAT_STRING) {
                // Must copy before the next wait_event: data is not ours.
                const char *s = *static_cast<char **>(prop->data);
                m_mediaTitle = s ? QString::fromUtf8(s) : QString();
                hasTitle = true;
            } else if (name == QLatin1String("hwdec-current") && prop->format == MPV_FORMAT_STRING) {
                const char *s = *static_cast<char **>(prop->data);
                m_hwdec = s ? QString::fromUtf8(s) : QString();
                hasHwdec = true;
            } else if (name == QLatin1String("track-list")) {
                // Observed with MPV_FORMAT_NONE, so prop->data is unset; the
                // value has to be fetched explicitly.
                refreshTracks();
                hasTracks = true;
            } else if (name == QLatin1String("sub-delay") && prop->format == MPV_FORMAT_DOUBLE) {
                m_subtitleDelay = *static_cast<double *>(prop->data);
                hasSubDelay = true;
            } else if (name == QLatin1String("sub-visibility") && prop->format == MPV_FORMAT_FLAG) {
                m_subtitlesVisible = *static_cast<int *>(prop->data) != 0;
                hasSubVisible = true;
            } else if (name == QLatin1String("sub-pos") && prop->format == MPV_FORMAT_DOUBLE) {
                m_subtitlePosition = *static_cast<double *>(prop->data);
                hasSubPos = true;
            } else if (name == QLatin1String("sub-font-size") && prop->format == MPV_FORMAT_DOUBLE) {
                m_subtitleFontSize = *static_cast<double *>(prop->data);
                hasSubFontSize = true;
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

    if (hasPos)    emit positionChanged();
    if (hasDur)    emit durationChanged();
    if (hasPause)  emit pausedChanged();
    if (hasVolume) emit volumeChanged();
    if (hasMute)   emit mutedChanged();
    if (hasSpeed)  emit speedChanged();
    if (hasTitle)  emit mediaTitleChanged();
    if (hasHwdec)  emit hwdecChanged();
    if (hasTracks) emit tracksChanged();
    if (hasSubDelay)     emit subtitleDelayChanged();
    if (hasSubVisible)   emit subtitlesVisibleChanged();
    if (hasSubPos)       emit subtitlePositionChanged();
    if (hasSubFontSize)  emit subtitleFontSizeChanged();
}

/// Reads "track-list" as an mpv_node, walks the node_list (keys is NULL for
/// arrays, valid for maps), copies everything into QVariantMaps and frees the
/// mpv-owned strings with mpv_free_node_contents().
void MpvObject::refreshTracks()
{
    if (!m_mpv)
        return;

    mpv_node node;
    if (mpv_get_property(m_mpv, "track-list", MPV_FORMAT_NODE, &node) < 0)
        return;

    QVariantList out;
    if (node.format == MPV_FORMAT_NODE_ARRAY && node.u.list) {
        mpv_node_list *list = node.u.list;
        for (int i = 0; i < list->num; ++i) {
            mpv_node &entry = list->values[i];
            if (entry.format != MPV_FORMAT_NODE_MAP || !entry.u.list)
                continue;
            mpv_node_list *fields = entry.u.list;
            QVariantMap track;
            for (int f = 0; f < fields->num; ++f) {
                const char *key = fields->keys ? fields->keys[f] : nullptr;
                if (!key)
                    continue;
                const QVariant v = nodeToVariant(fields->values[f]);
                if (v.isValid())
                    track.insert(QString::fromUtf8(key), v);
            }
            if (!track.isEmpty())
                out.append(track);
        }
    }
    mpv_free_node_contents(&node);

    if (out != m_tracks)
        m_tracks = out;
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
    // mpv supports 0.01..100 (see `mpv --speed=` in the manual).
    if (!(speed >= kMinSpeed))
        speed = kMinSpeed;
    if (speed > kMaxSpeed)
        speed = kMaxSpeed;
    double s = speed;
    mpv_set_property(m_mpv, "speed", MPV_FORMAT_DOUBLE, &s);
}

void MpvObject::setVolume(int volume)
{
    if (!m_mpv)
        return;
    // mpv treats anything below 0 as 0; keep our own range at 0..100.
    if (volume < 0)
        volume = 0;
    if (volume > 100)
        volume = 100;
    int64_t v = volume;
    mpv_set_property(m_mpv, "volume", MPV_FORMAT_INT64, &v);
}

void MpvObject::setMuted(bool muted)
{
    if (!m_mpv)
        return;
    int flag = muted ? 1 : 0;
    mpv_set_property(m_mpv, "mute", MPV_FORMAT_FLAG, &flag);
}

void MpvObject::seekAbsolute(double seconds)
{
    if (!m_mpv)
        return;
    if (!(seconds >= 0.0))
        seconds = 0.0;
    const QByteArray arg = QByteArray::number(seconds, 'f', 3);
    const char *cmd[] = {"seek", arg.constData(), "absolute", nullptr};
    mpv_command(m_mpv, cmd);
}

void MpvObject::frameStep(int n)
{
    if (!m_mpv)
        return;
    if (n == 0)
        return;

    const QByteArray frames = QByteArray::number(n);
    if (n > 0) {
        // Forward: mpv plays n frames and pauses again ("play" mode).
        const char *cmd[] = {"frame-step", frames.constData(), "play", nullptr};
        mpv_command(m_mpv, cmd);
    } else {
        // Backward always needs a seek; -1 lands exactly on the previous frame.
        const char *cmd[] = {"frame-step", frames.constData(), "seek", nullptr};
        mpv_command(m_mpv, cmd);
    }
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

// ---- subtitle / track control ----------------------------------------------

void MpvObject::selectTrack(qint64 id)
{
    if (!m_mpv)
        return;

    // The id is only unique within its own track type, so find out which
    // property ("aid" or "sid") it belongs to.
    for (const QVariant &v : m_tracks) {
        const QVariantMap t = v.toMap();
        if (t.value(QStringLiteral("id")).toLongLong() != id)
            continue;
        const QString type = t.value(QStringLiteral("type")).toString();
        if (type == QLatin1String("audio"))
            mpv_set_property(m_mpv, "aid", MPV_FORMAT_INT64, &id);
        else if (type == QLatin1String("sub"))
            mpv_set_property(m_mpv, "sid", MPV_FORMAT_INT64, &id);
        return;
    }
}

void MpvObject::disableSubtitles()
{
    if (!m_mpv)
        return;
    // "sid" accepts the string "no" to deselect without unloading the track.
    mpv_set_property_string(m_mpv, "sid", "no");
}

void MpvObject::addSubtitleFile(const QString &path)
{
    if (!m_mpv || path.isEmpty())
        return;
    // The flags argument is a string ("select" = make it current right away).
    const QByteArray encoded = QFile::encodeName(path);
    const char *cmd[] = {"sub-add", encoded.constData(), "select", nullptr};
    if (mpv_command(m_mpv, cmd) < 0)
        std::fprintf(stderr, "parvaz: sub-add failed for %s\n", qPrintable(path));
}

void MpvObject::setSubtitleCodepage(const QString &codepage)
{
    if (!m_mpv || codepage.isEmpty())
        return;
    // Without a leading '+', mpv still trusts data that already looks like
    // UTF-8; only "+cp1256" would force the legacy decoder onto UTF-8 files.
    mpv_set_property_string(m_mpv, "sub-codepage", QFile::encodeName(codepage).constData());
    reloadSubtitles();
}

void MpvObject::reloadSubtitles()
{
    if (!m_mpv)
        return;
    const char *cmd[] = {"sub-reload", nullptr};
    mpv_command(m_mpv, cmd);
}

QString MpvObject::subtitleCodepage() const
{
    if (!m_mpv)
        return QString();
    char *cp = mpv_get_property_string(m_mpv, "sub-codepage");
    const QString out = cp ? QString::fromUtf8(cp) : QString();
    mpv_free(cp);
    return out;
}

QString MpvObject::subtitleText() const
{
    if (!m_mpv)
        return QString();
    char *text = mpv_get_property_string(m_mpv, "sub-text");
    const QString out = text ? QString::fromUtf8(text) : QString();
    mpv_free(text);
    return out;
}

void MpvObject::cycleSubtitleTrack()
{
    if (!m_mpv)
        return;
    const char *cmd[] = {"cycle", "sid", nullptr};
    mpv_command(m_mpv, cmd);
}

void MpvObject::cycleAudioTrack()
{
    if (!m_mpv)
        return;
    const char *cmd[] = {"cycle", "aid", nullptr};
    mpv_command(m_mpv, cmd);
}

QVariantList MpvObject::tracksOfType(const QString &type) const
{
    QVariantList out;
    int fallback = 0;
    for (const QVariant &v : m_tracks) {
        const QVariantMap t = v.toMap();
        if (t.value(QStringLiteral("type")).toString() != type)
            continue;

        QString label = t.value(QStringLiteral("title")).toString();
        const QString lang = t.value(QStringLiteral("lang")).toString();
        if (label.isEmpty())
            label = lang.isEmpty() ? QStringLiteral("Track %1").arg(++fallback)
                                   : QStringLiteral("%1 (%2)").arg(lang).arg(++fallback);
        else if (!lang.isEmpty())
            label = QStringLiteral("%1 [%2]").arg(label, lang);

        QVariantMap entry;
        entry.insert(QStringLiteral("id"), t.value(QStringLiteral("id")));
        entry.insert(QStringLiteral("label"), label);
        entry.insert(QStringLiteral("selected"), t.value(QStringLiteral("selected")).toBool());
        out.append(entry);
    }
    return out;
}

void MpvObject::setSubtitleDelay(double seconds)
{
    if (!m_mpv)
        return;
    double d = seconds;
    mpv_set_property(m_mpv, "sub-delay", MPV_FORMAT_DOUBLE, &d);
}

void MpvObject::setSubtitlesVisible(bool visible)
{
    if (!m_mpv)
        return;
    int flag = visible ? 1 : 0;
    mpv_set_property(m_mpv, "sub-visibility", MPV_FORMAT_FLAG, &flag);
}

void MpvObject::setSubtitlePosition(double percentFromTop)
{
    if (!m_mpv)
        return;
    // mpv's documented range is 0..150.
    double p = qBound(0.0, percentFromTop, 150.0);
    mpv_set_property(m_mpv, "sub-pos", MPV_FORMAT_DOUBLE, &p);
}

void MpvObject::setSubtitleFontSize(double size)
{
    if (!m_mpv)
        return;
    // mpv clamps to 1..9000 itself; keep a sane UI range.
    double s = qBound(8.0, size, 200.0);
    mpv_set_property(m_mpv, "sub-font-size", MPV_FORMAT_DOUBLE, &s);
}
