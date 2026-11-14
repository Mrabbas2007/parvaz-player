#include "mpv/MpvObject.h"
#include "playback/PlaybackHistory.h"

#include <QMetaObject>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QCollator>
#include <QSet>
#include <QUrl>
#include <QDebug>
#include <QVariantMap>

#include <algorithm>
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

QString aspectFromRatio(double ratio)
{
    if (ratio <= 0.0)
        return QStringLiteral("default");
    if (std::abs(ratio - (16.0 / 9.0)) < 0.02)
        return QStringLiteral("16:9");
    if (std::abs(ratio - (4.0 / 3.0)) < 0.02)
        return QStringLiteral("4:3");
    if (std::abs(ratio - 2.35) < 0.05)
        return QStringLiteral("2.35:1");
    return QString::number(ratio, 'f', 2);
}

bool isMediaFile(const QString &fileName)
{
    static const QSet<QString> mediaExts = {
        QStringLiteral("mp4"), QStringLiteral("mkv"), QStringLiteral("webm"),
        QStringLiteral("avi"), QStringLiteral("mov"), QStringLiteral("flv"),
        QStringLiteral("ts"),  QStringLiteral("m2ts"), QStringLiteral("wmv"),
        QStringLiteral("vob"), QStringLiteral("ogm"), QStringLiteral("3gp"),
        QStringLiteral("mp3"), QStringLiteral("flac"), QStringLiteral("m4a"),
        QStringLiteral("aac"), QStringLiteral("wav"), QStringLiteral("ogg"),
        QStringLiteral("opus"), QStringLiteral("wma"), QStringLiteral("alac"),
        QStringLiteral("aiff"), QStringLiteral("m3u"), QStringLiteral("m3u8"),
        QStringLiteral("pls")
    };
    int dotIdx = fileName.lastIndexOf('.');
    if (dotIdx < 0)
        return false;
    return mediaExts.contains(fileName.mid(dotIdx + 1).toLower());
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
    mpv_set_option_string(m_mpv, "volume-max", "150");
    // Place secondary subtitles at top of video (10% from top)
    mpv_set_option_string(m_mpv, "secondary-sub-pos", "10");

    if (mpv_initialize(m_mpv) < 0) {
        qFatal("parvaz: mpv_initialize failed");
        return;
    }
    m_subtitles.setHandle(m_mpv);

    // Subtitle state lives in the controller; re-expose its change signals
    // under the MpvObject NOTIFY names QML binds to.
    connect(&m_subtitles, &SubtitleController::delayChanged,
            this, &MpvObject::subtitleDelayChanged);
    connect(&m_subtitles, &SubtitleController::visibleChanged,
            this, &MpvObject::subtitlesVisibleChanged);
    connect(&m_subtitles, &SubtitleController::positionChanged,
            this, &MpvObject::subtitlePositionChanged);
    connect(&m_subtitles, &SubtitleController::fontSizeChanged,
            this, &MpvObject::subtitleFontSizeChanged);
    connect(&m_subtitles, &SubtitleController::secondaryTrackChanged,
            this, &MpvObject::secondarySubtitleTrackChanged);

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
    mpv_observe_property(m_mpv, 0, "video-aspect-override", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "playlist-pos",   MPV_FORMAT_INT64);
    mpv_observe_property(m_mpv, 0, "playlist-count", MPV_FORMAT_INT64);
    mpv_observe_property(m_mpv, 0, "playlist",       MPV_FORMAT_NONE);
    mpv_observe_property(m_mpv, 0, "chapter",        MPV_FORMAT_INT64);
    mpv_observe_property(m_mpv, 0, "chapter-list",   MPV_FORMAT_NONE);
    mpv_observe_property(m_mpv, 0, "secondary-sid",  MPV_FORMAT_INT64);
    mpv_observe_property(m_mpv, 0, "metadata",       MPV_FORMAT_NONE);

    // Apply cover-art extraction results to the current file only; the file
    // identity check here is the final guard against stale async results.
    connect(&m_coverArt, &parvaz::CoverArtProvider::extracted, this,
            [this](const QString &file, const QString &url) {
        if (url.isEmpty() || m_currentFile != file || !isAudioOnly())
            return;
        setCoverArtUrl(url);
    });

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
    bool hasAspect = false;
    bool hasPlaylist = false;
    bool hasChapter = false;
    bool hasChapters = false;

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

            // sub-delay/visibility/pos/font-size and secondary-sid belong to
            // the subtitle controller; it emits its change signals directly.
            if (m_subtitles.onMpvProperty(name, prop->format, prop->data))
                break;

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
            } else if (name == QLatin1String("video-aspect-override") && prop->format == MPV_FORMAT_DOUBLE) {
                m_videoAspect = aspectFromRatio(*static_cast<double *>(prop->data));
                hasAspect = true;
            } else if (name == QLatin1String("playlist-pos") && prop->format == MPV_FORMAT_INT64) {
                m_playlistIndex = static_cast<int>(*static_cast<int64_t *>(prop->data));
                hasPlaylist = true;
            } else if (name == QLatin1String("playlist-count") && prop->format == MPV_FORMAT_INT64) {
                m_playlistCount = static_cast<int>(*static_cast<int64_t *>(prop->data));
                hasPlaylist = true;
            } else if (name == QLatin1String("playlist")) {
                refreshPlaylist();
                hasPlaylist = true;
            } else if (name == QLatin1String("chapter") && prop->format == MPV_FORMAT_INT64) {
                m_currentChapter = static_cast<int>(*static_cast<int64_t *>(prop->data));
                hasChapter = true;
            } else if (name == QLatin1String("chapter-list")) {
                refreshChapters();
                hasChapters = true;
            } else if (name == QLatin1String("metadata")) {
                refreshMetadata();
            }
            break;
        }
        case MPV_EVENT_FILE_LOADED: {
            char *path = mpv_get_property_string(m_mpv, "path");
            if (path) {
                m_currentFile = QString::fromUtf8(path);
                mpv_free(path);
                emit currentFileChanged();
            }
            refreshTracks();
            hasTracks = true;
            emit hasVideoChanged();
            emit isAudioOnlyChanged();
            refreshPlaylist();
            hasPlaylist = true;
            refreshChapters();
            hasChapters = true;
            refreshBookmarks();
            refreshMetadata();
            refreshCoverArt();
            emit fileLoaded();
            break;
        }
        case MPV_EVENT_END_FILE:
            m_position = 0.0;
            hasPos = true;
            emit hasVideoChanged();
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
    if (hasAspect)       emit videoAspectChanged();
    if (hasPlaylist)     emit playlistChanged();
    if (hasChapter)      emit currentChapterChanged();
    if (hasChapters)     emit chaptersChanged();
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

    const bool oldHasVideo = hasVideo();
    const bool oldIsAudioOnly = isAudioOnly();
    if (out != m_tracks) {
        m_tracks = out;
        if (hasVideo() != oldHasVideo)
            emit hasVideoChanged();
        if (isAudioOnly() != oldIsAudioOnly)
            emit isAudioOnlyChanged();
    }
}

void MpvObject::refreshPlaylist()
{
    if (!m_mpv)
        return;

    mpv_node node;
    if (mpv_get_property(m_mpv, "playlist", MPV_FORMAT_NODE, &node) < 0)
        return;

    QVariantList out;
    if (node.format == MPV_FORMAT_NODE_ARRAY && node.u.list) {
        mpv_node_list *list = node.u.list;
        for (int i = 0; i < list->num; ++i) {
            mpv_node &entry = list->values[i];
            if (entry.format != MPV_FORMAT_NODE_MAP || !entry.u.list)
                continue;
            mpv_node_list *fields = entry.u.list;
            QVariantMap item;
            item.insert(QStringLiteral("index"), i);
            for (int f = 0; f < fields->num; ++f) {
                const char *key = fields->keys ? fields->keys[f] : nullptr;
                if (!key)
                    continue;
                const QVariant v = nodeToVariant(fields->values[f]);
                if (v.isValid())
                    item.insert(QString::fromUtf8(key), v);
            }
            const QString title = item.value(QStringLiteral("title")).toString();
            const QString filename = item.value(QStringLiteral("filename")).toString();
            if (title.isEmpty() && !filename.isEmpty()) {
                item.insert(QStringLiteral("title"), QFileInfo(filename).fileName());
            }
            if (!item.isEmpty())
                out.append(item);
        }
    }
    mpv_free_node_contents(&node);

    m_playlist = out;
    m_playlistCount = m_playlist.size();

    int64_t pos = -1;
    if (mpv_get_property(m_mpv, "playlist-pos", MPV_FORMAT_INT64, &pos) >= 0) {
        m_playlistIndex = static_cast<int>(pos);
    }
}

QStringList MpvObject::expandAndSortPaths(const QStringList &rawPaths)
{
    QStringList result;
    QCollator collator;
    collator.setNumericMode(true);

    for (const QString &raw : rawPaths) {
        if (raw.isEmpty())
            continue;

        QString path = raw;
        if (path.startsWith(QLatin1String("file://"))) {
            const QUrl url(path);
            path = url.toLocalFile();
        }

        const QFileInfo fi(path);
        if (fi.isDir()) {
            QDirIterator it(path, QDir::Files | QDir::Readable,
                            QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
            QStringList dirFiles;
            while (it.hasNext()) {
                const QString f = it.next();
                if (isMediaFile(f))
                    dirFiles.append(f);
            }
            std::sort(dirFiles.begin(), dirFiles.end(), collator);
            result.append(dirFiles);
        } else if (fi.isFile() || path.contains(QLatin1String("://"))) {
            result.append(path);
        }
    }
    return result;
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

    if (!m_pendingFiles.isEmpty()) {
        const QStringList pending = m_pendingFiles;
        const QString mode = m_pendingMode.isEmpty() ? QStringLiteral("replace") : m_pendingMode;
        m_pendingFiles.clear();
        m_pendingMode.clear();
        loadFiles(pending, mode);
    }
}

void MpvObject::loadFile(const QString &path)
{
    loadFiles(QStringList{path}, QStringLiteral("replace"));
}

void MpvObject::loadFiles(const QStringList &paths, const QString &mode)
{
    if (!m_mpv || paths.isEmpty())
        return;

    const QStringList expanded = expandAndSortPaths(paths);
    if (expanded.isEmpty())
        return;

    // Defer until the render context owns the video output.
    if (!m_renderReady) {
        m_pendingFiles = expanded;
        m_pendingMode = mode;
        m_currentFile = expanded.first();
        emit currentFileChanged();
        return;
    }

    const bool isAppend = (mode == QLatin1String("append"));
    bool firstItemHandled = false;

    for (int i = 0; i < expanded.size(); ++i) {
        const QString &file = expanded[i];
        const QByteArray encoded = QFile::encodeName(file);

        if (!isAppend && i == 0) {
            m_currentFile = file;
            emit currentFileChanged();
            const char *cmd[] = {"loadfile", encoded.constData(), "replace", nullptr};
            if (mpv_command(m_mpv, cmd) < 0)
                qWarning() << "parvaz: loadfile replace failed for" << file;
            firstItemHandled = true;
        } else {
            const char *loadFlag = "append";
            if (isAppend && !firstItemHandled && (m_playlistCount == 0 || m_duration <= 0.0)) {
                loadFlag = "append-play";
                firstItemHandled = true;
                m_currentFile = file;
                emit currentFileChanged();
            }
            const char *cmd[] = {"loadfile", encoded.constData(), loadFlag, nullptr};
            if (mpv_command(m_mpv, cmd) < 0)
                qWarning() << "parvaz: loadfile append failed for" << file;
        }
    }

    refreshPlaylist();
    emit playlistChanged();
}

void MpvObject::next()
{
    if (!m_mpv)
        return;
    const char *cmd[] = {"playlist-next", "weak", nullptr};
    mpv_command(m_mpv, cmd);
}

void MpvObject::previous()
{
    if (!m_mpv)
        return;
    const char *cmd[] = {"playlist-prev", "weak", nullptr};
    mpv_command(m_mpv, cmd);
}

void MpvObject::playIndex(int index)
{
    if (!m_mpv || index < 0)
        return;
    const QByteArray idxStr = QByteArray::number(index);
    const char *cmd[] = {"playlist-play-index", idxStr.constData(), nullptr};
    mpv_command(m_mpv, cmd);
}

void MpvObject::removeIndex(int index)
{
    if (!m_mpv || index < 0)
        return;
    const QByteArray idxStr = QByteArray::number(index);
    const char *cmd[] = {"playlist-remove", idxStr.constData(), nullptr};
    mpv_command(m_mpv, cmd);
}

void MpvObject::clearPlaylist()
{
    if (!m_mpv)
        return;
    const char *cmd[] = {"playlist-clear", nullptr};
    mpv_command(m_mpv, cmd);
}

void MpvObject::shufflePlaylist()
{
    if (!m_mpv)
        return;
    const char *cmd[] = {"playlist-shuffle", nullptr};
    mpv_command(m_mpv, cmd);
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

bool MpvObject::isAudioOnly() const
{
    bool hasRealVideo = false;
    bool hasAudio = false;

    for (const QVariant &v : m_tracks) {
        const QVariantMap m = v.toMap();
        const QString type = m.value(QStringLiteral("type")).toString();
        if (type == QLatin1String("video")) {
            const bool isAlbumArt = m.value(QStringLiteral("albumart")).toBool() ||
                                   m.value(QStringLiteral("image")).toBool();
            if (!isAlbumArt)
                hasRealVideo = true;
        } else if (type == QLatin1String("audio")) {
            hasAudio = true;
        }
    }

    if (hasRealVideo)
        return false;

    if (hasAudio)
        return true;

    if (!m_currentFile.isEmpty()) {
        const QString lower = m_currentFile.toLower();
        return lower.endsWith(QLatin1String(".mp3")) ||
               lower.endsWith(QLatin1String(".flac")) ||
               lower.endsWith(QLatin1String(".ogg")) ||
               lower.endsWith(QLatin1String(".opus")) ||
               lower.endsWith(QLatin1String(".wav")) ||
               lower.endsWith(QLatin1String(".m4a")) ||
               lower.endsWith(QLatin1String(".aac")) ||
               lower.endsWith(QLatin1String(".wma"));
    }

    return false;
}

bool MpvObject::hasVideo() const
{
    if (isAudioOnly())
        return false;

    for (const QVariant &v : m_tracks) {
        const QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("type")).toString() == QLatin1String("video")) {
            if (!m.value(QStringLiteral("albumart")).toBool() && !m.value(QStringLiteral("image")).toBool())
                return true;
        }
    }
    return false;
}

QString MpvObject::trackTitle() const
{
    if (m_metadata.contains(QStringLiteral("title"))) {
        const QString t = m_metadata.value(QStringLiteral("title")).toString().trimmed();
        if (!t.isEmpty())
            return t;
    }
    if (!m_mediaTitle.isEmpty()) {
        return m_mediaTitle;
    }
    if (!m_currentFile.isEmpty()) {
        return QFileInfo(m_currentFile).completeBaseName();
    }
    return QString();
}

QString MpvObject::trackArtist() const
{
    if (m_metadata.contains(QStringLiteral("artist"))) {
        const QString a = m_metadata.value(QStringLiteral("artist")).toString().trimmed();
        if (!a.isEmpty())
            return a;
    }
    if (m_metadata.contains(QStringLiteral("album_artist"))) {
        return m_metadata.value(QStringLiteral("album_artist")).toString().trimmed();
    }
    return QString();
}

QString MpvObject::trackAlbum() const
{
    if (m_metadata.contains(QStringLiteral("album"))) {
        return m_metadata.value(QStringLiteral("album")).toString().trimmed();
    }
    return QString();
}

QString MpvObject::trackYear() const
{
    if (m_metadata.contains(QStringLiteral("date"))) {
        const QString d = m_metadata.value(QStringLiteral("date")).toString().trimmed();
        if (!d.isEmpty())
            return d.left(4);
    }
    if (m_metadata.contains(QStringLiteral("year"))) {
        return m_metadata.value(QStringLiteral("year")).toString().trimmed();
    }
    return QString();
}

QString MpvObject::trackGenre() const
{
    if (m_metadata.contains(QStringLiteral("genre"))) {
        return m_metadata.value(QStringLiteral("genre")).toString().trimmed();
    }
    return QString();
}

void MpvObject::refreshMetadata()
{
    if (!m_mpv)
        return;

    QVariantMap meta;
    mpv_node node;
    if (mpv_get_property(m_mpv, "metadata", MPV_FORMAT_NODE, &node) >= 0) {
        if (node.format == MPV_FORMAT_NODE_MAP && node.u.list) {
            mpv_node_list *list = node.u.list;
            for (int i = 0; i < list->num; ++i) {
                const char *key = list->keys ? list->keys[i] : nullptr;
                if (!key)
                    continue;
                const QVariant v = nodeToVariant(list->values[i]);
                if (v.isValid()) {
                    meta.insert(QString::fromUtf8(key).toLower(), v);
                }
            }
        }
        mpv_free_node_contents(&node);
    }
    if (meta != m_metadata) {
        m_metadata = meta;
        emit metadataChanged();
    }
}

void MpvObject::setCoverArtUrl(const QString &url)
{
    if (m_coverArtUrl == url)
        return;
    m_coverArtUrl = url;
    emit coverArtUrlChanged();
}

void MpvObject::refreshCoverArt()
{
    if (m_currentFile.isEmpty() || !isAudioOnly()) {
        // Stop a stale extraction for a previous file before clearing.
        m_coverArt.cancel();
        setCoverArtUrl(QString());
        return;
    }

    const QString url = m_coverArt.localCoverUrl(m_currentFile);
    if (!url.isEmpty()) {
        setCoverArtUrl(url);
        return;
    }

    // Nothing cached: the provider extracts asynchronously (an in-flight
    // ffmpeg pass would otherwise freeze the GUI thread for up to ~2.5s on
    // every audio file without a folder cover). The extracted() connection
    // in the constructor applies the result to the current file.
    m_coverArt.requestExtraction(m_currentFile);
}

void MpvObject::setVolume(int volume)
{
    if (!m_mpv)
        return;
    // mpv supports volume boosting; allow range 0..150.
    volume = qBound(0, volume, 150);
    if (m_volume != volume) {
        m_volume = volume;
        emit volumeChanged();
    }
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
// Subtitle state and mpv calls live in SubtitleController; these are the
// QML-facing delegates (plus OSD announcements).

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
    m_subtitles.deselect();
}

void MpvObject::addSubtitleFile(const QString &path)
{
    m_subtitles.addSubtitleFile(path);
}

void MpvObject::setSubtitleCodepage(const QString &codepage)
{
    m_subtitles.setCodepage(codepage);
}

void MpvObject::reloadSubtitles()
{
    m_subtitles.reload();
}

QString MpvObject::subtitleCodepage() const
{
    return m_subtitles.codepage();
}

QString MpvObject::subtitleText() const
{
    return m_subtitles.text();
}

void MpvObject::cycleSubtitleTrack()
{
    m_subtitles.cycleTrack();
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
    m_subtitles.setDelay(seconds);
}

void MpvObject::setSubtitlesVisible(bool visible)
{
    m_subtitles.setVisible(visible);
}

void MpvObject::setSubtitlePosition(double percentFromTop)
{
    m_subtitles.setPosition(percentFromTop);
}

void MpvObject::setBottomBarOffset(bool visible, double barHeight, double windowHeight)
{
    m_subtitles.setBottomBarOffset(visible, barHeight, windowHeight);
}

void MpvObject::setSubtitleFontSize(double size)
{
    m_subtitles.setFontSize(size);
}

void MpvObject::adjustSpeed(double delta)
{
    double newSpeed = qBound(kMinSpeed, m_speed + delta, kMaxSpeed);
    // Round to 1 decimal place to keep it clean (e.g. 0.9, 1.0, 1.1)
    newSpeed = std::round(newSpeed * 10.0) / 10.0;
    setSpeed(newSpeed);
    showOsd(QStringLiteral("Speed: %1x").arg(m_speed, 0, 'f', 2));
}

void MpvObject::resetSpeed()
{
    setSpeed(1.0);
    showOsd(QStringLiteral("Speed: 1.00x"));
}

void MpvObject::adjustVolume(int delta)
{
    if (m_muted && delta > 0)
        setMuted(false);
    int newVol = qBound(0, m_volume + delta, 150);
    setVolume(newVol);
    showOsd(m_muted ? QStringLiteral("Mute") : QStringLiteral("Volume: %1%").arg(m_volume));
}

void MpvObject::adjustSubtitlePosition(double delta)
{
    m_subtitles.setPosition(m_subtitles.position() + delta);
    showOsd(QStringLiteral("Sub pos: %1%").arg(int(std::round(m_subtitles.position()))));
}

void MpvObject::adjustSubtitleFontSize(double delta)
{
    m_subtitles.setFontSize(m_subtitles.fontSize() + delta);
    showOsd(QStringLiteral("Sub size: %1").arg(int(std::round(m_subtitles.fontSize()))));
}

void MpvObject::showOsd(const QString &text)
{
    emit osdRequested(text);
}

void MpvObject::setVideoAspect(const QString &aspect)
{
    if (!m_mpv)
        return;
    double ratio = -1.0;
    if (aspect == QLatin1String("16:9"))
        ratio = 16.0 / 9.0;
    else if (aspect == QLatin1String("4:3"))
        ratio = 4.0 / 3.0;
    else if (aspect == QLatin1String("2.35:1"))
        ratio = 2.35;

    mpv_set_property(m_mpv, "video-aspect-override", MPV_FORMAT_DOUBLE, &ratio);
    m_videoAspect = aspectFromRatio(ratio);
    emit videoAspectChanged();
    showOsd(QStringLiteral("Aspect ratio: %1").arg(m_videoAspect));
}

void MpvObject::setHistory(parvaz::PlaybackHistory *history)
{
    m_history = history;
    refreshBookmarks();
}

void MpvObject::addBookmark(const QString &title)
{
    if (m_currentFile.isEmpty() || !m_history)
        return;

    QString t = title;
    if (t.isEmpty()) {
        t = QStringLiteral("Bookmark (%1)").arg(formatTime(m_position));
    }
    m_history->addBookmark(m_currentFile, m_position, t);
    refreshBookmarks();
    showOsd(QStringLiteral("★ %1 added").arg(t));
}

void MpvObject::removeBookmark(int id)
{
    if (!m_history)
        return;
    m_history->removeBookmark(id);
    refreshBookmarks();
}

void MpvObject::clearBookmarks()
{
    if (m_currentFile.isEmpty() || !m_history)
        return;
    m_history->clearBookmarks(m_currentFile);
    refreshBookmarks();
    showOsd(QStringLiteral("Bookmarks cleared"));
}

void MpvObject::refreshBookmarks()
{
    if (m_currentFile.isEmpty() || !m_history) {
        if (!m_bookmarks.isEmpty()) {
            m_bookmarks.clear();
            emit bookmarksChanged();
        }
        return;
    }

    const QVariantList b = m_history->bookmarksForFile(m_currentFile);
    if (b != m_bookmarks) {
        m_bookmarks = b;
        emit bookmarksChanged();
    }
}

void MpvObject::refreshChapters()
{
    if (!m_mpv)
        return;

    mpv_node node;
    if (mpv_get_property(m_mpv, "chapter-list", MPV_FORMAT_NODE, &node) < 0)
        return;

    QVariantList out;
    if (node.format == MPV_FORMAT_NODE_ARRAY && node.u.list) {
        mpv_node_list *list = node.u.list;
        for (int i = 0; i < list->num; ++i) {
            mpv_node &entry = list->values[i];
            if (entry.format != MPV_FORMAT_NODE_MAP || !entry.u.list)
                continue;
            mpv_node_list *fields = entry.u.list;
            QVariantMap ch;
            ch.insert(QStringLiteral("index"), i);
            for (int f = 0; f < fields->num; ++f) {
                const char *key = fields->keys ? fields->keys[f] : nullptr;
                if (!key)
                    continue;
                const QVariant v = nodeToVariant(fields->values[f]);
                if (v.isValid())
                    ch.insert(QString::fromUtf8(key), v);
            }
            if (!ch.contains(QStringLiteral("title")) || ch.value(QStringLiteral("title")).toString().isEmpty()) {
                ch.insert(QStringLiteral("title"), QStringLiteral("Chapter %1").arg(i + 1));
            }
            out.append(ch);
        }
    }
    mpv_free_node_contents(&node);

    if (out != m_chapters) {
        m_chapters = out;
        emit chaptersChanged();
    }
}

void MpvObject::nextChapter()
{
    if (!m_mpv)
        return;
    const char *cmd[] = {"add", "chapter", "1", nullptr};
    mpv_command_async(m_mpv, 0, cmd);
}

void MpvObject::previousChapter()
{
    if (!m_mpv)
        return;
    const char *cmd[] = {"add", "chapter", "-1", nullptr};
    mpv_command_async(m_mpv, 0, cmd);
}

void MpvObject::setChapter(int index)
{
    if (!m_mpv || index < 0)
        return;
    int64_t idx = index;
    mpv_set_property_async(m_mpv, 0, "chapter", MPV_FORMAT_INT64, &idx);
}

void MpvObject::selectSecondarySubtitleTrack(qint64 id)
{
    m_subtitles.selectSecondaryTrack(id);
    if (id <= 0)
        showOsd(QStringLiteral("Secondary sub: None"));
    else
        showOsd(QStringLiteral("Secondary sub track #%1").arg(id));
}

void MpvObject::cycleSecondarySubtitleTrack()
{
    m_subtitles.cycleSecondaryTrack();
}

void MpvObject::setAudioNormalizer(bool enabled)
{
    if (m_audioNormalizer == enabled)
        return;
    m_audioNormalizer = enabled;
    emit audioNormalizerChanged();

    if (!m_mpv)
        return;
    if (enabled) {
        const char *cmd[] = {"af", "add", "@norm:lavfi=[dynaudnorm=f=250:g=31:p=0.95:m=10.0:r=0.9]", nullptr};
        mpv_command_async(m_mpv, 0, cmd);
        showOsd(QStringLiteral("Night Mode (Normalizer): ON"));
    } else {
        const char *cmd[] = {"af", "remove", "@norm", nullptr};
        mpv_command_async(m_mpv, 0, cmd);
        showOsd(QStringLiteral("Night Mode (Normalizer): OFF"));
    }
}

void MpvObject::toggleAudioNormalizer()
{
    setAudioNormalizer(!m_audioNormalizer);
}

QVariantMap MpvObject::technicalStats() const
{
    QVariantMap map;
    if (!m_mpv)
        return map;

    auto getString = [this](const char *name) -> QString {
        char *s = mpv_get_property_string(m_mpv, name);
        if (!s)
            return QString();
        QString res = QString::fromUtf8(s);
        mpv_free(s);
        return res;
    };
    auto getInt = [this](const char *name) -> qint64 {
        int64_t v = 0;
        if (mpv_get_property(m_mpv, name, MPV_FORMAT_INT64, &v) < 0)
            return 0;
        return v;
    };
    auto getDouble = [this](const char *name) -> double {
        double v = 0.0;
        if (mpv_get_property(m_mpv, name, MPV_FORMAT_DOUBLE, &v) < 0)
            return 0.0;
        return v;
    };

    // Video specs
    QString vCodec = getString("video-codec");
    if (vCodec.isEmpty())
        vCodec = getString("video-format");
    qint64 width = getInt("video-params/w");
    qint64 height = getInt("video-params/h");
    double aspect = getDouble("video-params/aspect");
    double fps = getDouble("container-fps");
    if (fps <= 0.0)
        fps = getDouble("estimated-vf-fps");
    double vBitrate = getDouble("video-bitrate");
    QString hwdec = getString("hwdec-current");

    // Audio specs
    QString aCodec = getString("audio-codec-name");
    if (aCodec.isEmpty())
        aCodec = getString("audio-codec");
    qint64 sampleRate = getInt("audio-params/samplerate");
    qint64 channels = getInt("audio-params/channel-count");
    double aBitrate = getDouble("audio-bitrate");

    // Drops & Cache
    qint64 voDrops = getInt("frame-drop-count");
    qint64 decDrops = getInt("decoder-frame-drop-count");
    double cacheSec = getDouble("demuxer-cache-duration");
    double avsync = getDouble("avsync");

    map.insert(QStringLiteral("videoCodec"), vCodec.isEmpty() ? QStringLiteral("N/A") : vCodec.toUpper());
    map.insert(QStringLiteral("width"), width);
    map.insert(QStringLiteral("height"), height);
    map.insert(QStringLiteral("resolution"), (width > 0 && height > 0) ? QStringLiteral("%1x%2").arg(width).arg(height) : QStringLiteral("N/A"));
    map.insert(QStringLiteral("aspect"), aspect > 0.0 ? QString::number(aspect, 'f', 2) : QStringLiteral("N/A"));
    map.insert(QStringLiteral("fps"), fps > 0.0 ? QStringLiteral("%1 fps").arg(QString::number(fps, 'f', 3)) : QStringLiteral("N/A"));
    map.insert(QStringLiteral("hwdec"), (hwdec.isEmpty() || hwdec == QLatin1String("no")) ? QStringLiteral("Software (CPU)") : hwdec.toUpper());
    map.insert(QStringLiteral("isHwdec"), !hwdec.isEmpty() && hwdec != QLatin1String("no"));
    map.insert(QStringLiteral("videoBitrate"), vBitrate > 0.0 ? QStringLiteral("%1 kbps").arg(static_cast<int>(vBitrate / 1000.0)) : QStringLiteral("N/A"));

    map.insert(QStringLiteral("audioCodec"), aCodec.isEmpty() ? QStringLiteral("N/A") : aCodec.toUpper());
    map.insert(QStringLiteral("audioChannels"), channels > 0 ? (channels == 2 ? QStringLiteral("2 ch (Stereo)") : (channels == 6 ? QStringLiteral("6 ch (5.1)") : QStringLiteral("%1 ch").arg(channels))) : QStringLiteral("N/A"));
    map.insert(QStringLiteral("sampleRate"), sampleRate > 0 ? QStringLiteral("%1 Hz").arg(sampleRate) : QStringLiteral("N/A"));
    map.insert(QStringLiteral("audioBitrate"), aBitrate > 0.0 ? QStringLiteral("%1 kbps").arg(static_cast<int>(aBitrate / 1000.0)) : QStringLiteral("N/A"));
    map.insert(QStringLiteral("audioNormalizer"), m_audioNormalizer ? QStringLiteral("Active (dynaudnorm)") : QStringLiteral("Off"));

    map.insert(QStringLiteral("voDrops"), voDrops);
    map.insert(QStringLiteral("decDrops"), decDrops);
    map.insert(QStringLiteral("cacheDuration"), cacheSec > 0.0 ? QStringLiteral("%1s").arg(QString::number(cacheSec, 'f', 1)) : QStringLiteral("0.0s"));
    map.insert(QStringLiteral("avsync"), QStringLiteral("%1s").arg(QString::number(avsync, 'f', 3)));
    map.insert(QStringLiteral("speed"), QStringLiteral("%1x").arg(QString::number(m_speed, 'f', 2)));

    return map;
}
