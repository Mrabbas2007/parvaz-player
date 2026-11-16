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
#include <QClipboard>
#include <QGuiApplication>
#include <QStandardPaths>

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
    // Default to auto-copy: hardware-accelerated decode with safe copyback to GL textures.
    // Works reliably across Intel VAAPI/QSV, AMD VAAPI, NVIDIA NVDEC, and Vulkan on Linux/Windows.
    mpv_set_option_string(m_mpv, "hwdec", "auto-copy");
    mpv_set_option_string(m_mpv, "hwdec-codecs", "all");
    mpv_set_option_string(m_mpv, "vd-lavc-dr", "yes");   // Direct rendering in FFmpeg
    mpv_set_option_string(m_mpv, "framedrop", "vo");     // Drop late frames at VO to preserve audio sync
    mpv_set_option_string(m_mpv, "video-sync", "audio"); // Audio clock as master reference
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
    mpv_set_option_string(m_mpv, "target-colorspace-hint", "yes");
    mpv_set_option_string(m_mpv, "hdr-compute-peak", "auto"); // Use static HDR metadata when available

    // Adaptive demuxer cache: smooth readahead preventing buffer starvation on high-bitrate media
    mpv_set_option_string(m_mpv, "demuxer-max-bytes", "96MiB");
    mpv_set_option_string(m_mpv, "demuxer-max-back-bytes", "32MiB");
    mpv_set_option_string(m_mpv, "demuxer-readahead-secs", "10");
    mpv_set_option_string(m_mpv, "demuxer-hysteresis-secs", "3");
    mpv_set_option_string(m_mpv, "vd-lavc-threads", "0"); // auto detect all available CPU cores

    if (mpv_initialize(m_mpv) < 0) {
        qFatal("parvaz: mpv_initialize failed");
        return;
    }
    m_subtitles.setHandle(m_mpv);
    m_screenshot.setHandle(m_mpv);

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
    mpv_observe_property(m_mpv, 0, "ab-loop-a",      MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "ab-loop-b",      MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "brightness",    MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "contrast",      MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "saturation",    MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "hue",           MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "gamma",         MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "video-rotate",   MPV_FORMAT_INT64);
    mpv_observe_property(m_mpv, 0, "paused-for-cache",      MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 0, "cache-buffering-state", MPV_FORMAT_INT64);
    mpv_observe_property(m_mpv, 0, "seeking",               MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 0, "core-idle",             MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 0, "eof-reached",           MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 0, "deband",                MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 0, "tone-mapping",          MPV_FORMAT_STRING);
    mpv_observe_property(m_mpv, 0, "sharpen",               MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "video-params",          MPV_FORMAT_NODE);

    // Apply cover-art extraction results to the current file only; the file
    // identity check here is the final guard against stale async results.
    connect(&m_coverArt, &parvaz::CoverArtProvider::extracted, this,
            [this](const QString &file, const QString &url) {
        if (m_currentFile != file || !isAudioOnly())
            return;
        setCoverArtUrl(url);
    });

    // Subtitle search & download wiring
    connect(&m_subtitleSearch, &SubtitleSearchService::subtitleReadyToApply,
            this, [this](const QString &path) {
        addSubtitleFile(path);
        showOsd(QStringLiteral("Sub loaded: %1").arg(QFileInfo(path).fileName()));
    });

    // Subtitle auto-sync wiring
    connect(&m_subtitleSync, &SubtitleSyncService::syncFinished,
            this, [this](const QString &path) {
        addSubtitleFile(path);
        showOsd(QStringLiteral("Sub synchronized successfully!"));
    });
    connect(&m_subtitleSync, &SubtitleSyncService::syncFailed,
            this, [this](const QString &err) {
        showOsd(QStringLiteral("Sync error: %1").arg(err));
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
    const bool wasBuffering = isBuffering();

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
                // Echo of our own setMuted() is a no-op thanks to the check.
                const bool val = *static_cast<int *>(prop->data) != 0;
                if (val != m_muted) {
                    m_muted = val;
                    hasMute = true;
                }
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
                applyPerformanceProfile();
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
            } else if (name == QLatin1String("ab-loop-a")) {
                m_abLoopA = (prop->format == MPV_FORMAT_DOUBLE) ? *static_cast<double *>(prop->data) : -1.0;
                emit abLoopChanged();
            } else if (name == QLatin1String("ab-loop-b")) {
                m_abLoopB = (prop->format == MPV_FORMAT_DOUBLE) ? *static_cast<double *>(prop->data) : -1.0;
                emit abLoopChanged();
            } else if (name == QLatin1String("brightness") && prop->format == MPV_FORMAT_DOUBLE) {
                const double v = *static_cast<double *>(prop->data);
                if (!qFuzzyCompare(m_brightness, v)) {
                    m_brightness = v;
                    emit brightnessChanged();
                }
            } else if (name == QLatin1String("contrast") && prop->format == MPV_FORMAT_DOUBLE) {
                const double v = *static_cast<double *>(prop->data);
                if (!qFuzzyCompare(m_contrast, v)) {
                    m_contrast = v;
                    emit contrastChanged();
                }
            } else if (name == QLatin1String("saturation") && prop->format == MPV_FORMAT_DOUBLE) {
                const double v = *static_cast<double *>(prop->data);
                if (!qFuzzyCompare(m_saturation, v)) {
                    m_saturation = v;
                    emit saturationChanged();
                }
            } else if (name == QLatin1String("hue") && prop->format == MPV_FORMAT_DOUBLE) {
                const double v = *static_cast<double *>(prop->data);
                if (!qFuzzyCompare(m_hue, v)) {
                    m_hue = v;
                    emit hueChanged();
                }
            } else if (name == QLatin1String("gamma") && prop->format == MPV_FORMAT_DOUBLE) {
                const double v = *static_cast<double *>(prop->data);
                if (!qFuzzyCompare(m_gamma, v)) {
                    m_gamma = v;
                    emit gammaChanged();
                }
            } else if (name == QLatin1String("video-rotate") && prop->format == MPV_FORMAT_INT64) {
                const int r = static_cast<int>(*static_cast<int64_t *>(prop->data));
                if (m_videoRotate != r) {
                    m_videoRotate = r;
                    emit videoRotateChanged();
                }
            } else if (name == QLatin1String("paused-for-cache") && prop->format == MPV_FORMAT_FLAG) {
                m_pausedForCache = *static_cast<int *>(prop->data) != 0;
            } else if (name == QLatin1String("cache-buffering-state") && prop->format == MPV_FORMAT_INT64) {
                const int pct = static_cast<int>(*static_cast<int64_t *>(prop->data));
                if (m_bufferPercent != pct) {
                    m_bufferPercent = pct;
                    emit bufferPercentChanged();
                }
            } else if (name == QLatin1String("seeking") && prop->format == MPV_FORMAT_FLAG) {
                m_seeking = *static_cast<int *>(prop->data) != 0;
            } else if (name == QLatin1String("core-idle") && prop->format == MPV_FORMAT_FLAG) {
                m_coreIdle = *static_cast<int *>(prop->data) != 0;
            } else if (name == QLatin1String("eof-reached") && prop->format == MPV_FORMAT_FLAG) {
                const bool reached = *static_cast<int *>(prop->data) != 0;
                if (m_eofReached != reached) {
                    m_eofReached = reached;
                    emit eofReachedChanged();
                }
            } else if (name == QLatin1String("deband") && prop->format == MPV_FORMAT_FLAG) {
                const bool d = *static_cast<int *>(prop->data) != 0;
                if (m_debandEnabled != d) {
                    m_debandEnabled = d;
                    emit debandChanged();
                }
            } else if (name == QLatin1String("tone-mapping") && prop->format == MPV_FORMAT_STRING) {
                const char *s = *static_cast<char **>(prop->data);
                const QString t = s ? QString::fromUtf8(s) : QStringLiteral("auto");
                if (m_hdrToneMapping != t) {
                    m_hdrToneMapping = t;
                    emit hdrToneMappingChanged();
                }
            } else if (name == QLatin1String("sharpen") && prop->format == MPV_FORMAT_DOUBLE) {
                const double v = *static_cast<double *>(prop->data);
                if (!qFuzzyCompare(m_videoSharpen, v)) {
                    m_videoSharpen = v;
                    emit videoSharpenChanged();
                }
            } else if (name == QLatin1String("video-params")) {
                int64_t w = 0, h = 0;
                mpv_get_property(m_mpv, "video-params/w", MPV_FORMAT_INT64, &w);
                mpv_get_property(m_mpv, "video-params/h", MPV_FORMAT_INT64, &h);
                const bool is4kNow = (w >= 2560 || h >= 1440);
                if (is4kNow != m_is4k) {
                    m_is4k = is4kNow;
                    emit is4kChanged();
                }
                applyPerformanceProfile();
                emit technicalStatsChanged();
            }
            break;
        }
        case MPV_EVENT_START_FILE:
            m_isLoading = true;
            m_eofReached = false;
            m_bufferPercent = 0;
            emit loadingChanged();
            emit eofReachedChanged();
            setCoverArtUrl(QString());
            break;
        case MPV_EVENT_FILE_LOADED: {
            m_isLoading = false;
            m_eofReached = false;
            m_bufferPercent = 100;
            emit loadingChanged();
            emit eofReachedChanged();
            char *path = mpv_get_property_string(m_mpv, "path");
            if (path) {
                m_currentFile = QString::fromUtf8(path);
                mpv_free(path);
                emit currentFileChanged();
            }
            m_abLoopA = -1.0;
            m_abLoopB = -1.0;
            emit abLoopChanged();
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
            int64_t w = 0, h = 0;
            mpv_get_property(m_mpv, "video-params/w", MPV_FORMAT_INT64, &w);
            mpv_get_property(m_mpv, "video-params/h", MPV_FORMAT_INT64, &h);
            const bool is4kNow = (w >= 2560 || h >= 1440);
            if (is4kNow != m_is4k) {
                m_is4k = is4kNow;
                emit is4kChanged();
            }
            applyPerformanceProfile();
            emit fileLoaded();
            break;
        }
        case MPV_EVENT_VIDEO_RECONFIG: {
            int64_t w = 0, h = 0;
            mpv_get_property(m_mpv, "video-params/w", MPV_FORMAT_INT64, &w);
            mpv_get_property(m_mpv, "video-params/h", MPV_FORMAT_INT64, &h);
            const bool is4kNow = (w >= 2560 || h >= 1440);
            if (is4kNow != m_is4k) {
                m_is4k = is4kNow;
                emit is4kChanged();
            }
            applyPerformanceProfile();
            emit technicalStatsChanged();
            break;
        }
        case MPV_EVENT_END_FILE:
            m_isLoading = false;
            m_pausedForCache = false;
            m_seeking = false;
            m_bufferPercent = 100;
            emit loadingChanged();
            m_coverArt.cancel();
            setCoverArtUrl(QString());
            m_position = 0.0;
            hasPos = true;
            m_abLoopA = -1.0;
            m_abLoopB = -1.0;
            emit abLoopChanged();
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
    if (isBuffering() != wasBuffering) emit bufferingChanged();
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

        qint64 curAudio = 0;
        qint64 curSub = 0;
        for (const QVariant &v : out) {
            const QVariantMap t = v.toMap();
            if (t.value(QStringLiteral("selected")).toBool()) {
                const QString type = t.value(QStringLiteral("type")).toString();
                if (type == QLatin1String("audio"))
                    curAudio = t.value(QStringLiteral("id")).toLongLong();
                else if (type == QLatin1String("sub"))
                    curSub = t.value(QStringLiteral("id")).toLongLong();
            }
        }
        m_currentAudioTrack = curAudio;
        m_currentSubtitleTrack = curSub;

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
    if (!paused) {
        // If at EOF (or within 0.5s of end), rewind to start before unpausing.
        if (m_eofReached || (m_duration > 0.0 && m_position >= m_duration - 0.5)) {
            seekAbsolute(0.0);
            m_eofReached = false;
        }
    }
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

    if (!isAppend) {
        m_isLoading = true;
        m_bufferPercent = 0;
        emit loadingChanged();
        emit bufferingChanged();
    }

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
    if (index == m_playlistIndex && (m_eofReached || (m_duration > 0.0 && m_position >= m_duration - 0.5))) {
        seekAbsolute(0.0);
        m_eofReached = false;
        setPaused(false);
        return;
    }
    m_eofReached = false;
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
    if (m_eofReached || (m_duration > 0.0 && m_position >= m_duration - 0.5)) {
        seekAbsolute(0.0);
        m_eofReached = false;
        setPaused(false);
        return;
    }
    setPaused(!m_paused);
}

void MpvObject::seekRelative(double seconds)
{
    if (!m_mpv)
        return;
    m_eofReached = false;
    const QByteArray arg = QByteArray::number(seconds, 'f', 3);
    const char *cmd[] = {"seek", arg.constData(), "relative", nullptr};
    if (mpv_command(m_mpv, cmd) >= 0)
        emit seeked(m_position + seconds);
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
    // Always clear existing cover art first so previous tracks never leak
    m_coverArt.cancel();
    setCoverArtUrl(QString());

    if (m_currentFile.isEmpty() || !isAudioOnly()) {
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
    // Update local state optimistically (like setVolume): the QML icon and
    // the toggle logic must never depend on the mpv echo arriving. The echo
    // below is filtered in drainEvents so no duplicate signal is emitted.
    if (m_muted != muted) {
        m_muted = muted;
        emit mutedChanged();
    }
    int flag = muted ? 1 : 0;
    mpv_set_property(m_mpv, "mute", MPV_FORMAT_FLAG, &flag);
}

void MpvObject::seekAbsolute(double seconds)
{
    if (!m_mpv)
        return;
    if (!(seconds >= 0.0))
        seconds = 0.0;
    m_eofReached = false;
    const QByteArray arg = QByteArray::number(seconds, 'f', 3);
    const char *cmd[] = {"seek", arg.constData(), "absolute", nullptr};
    if (mpv_command(m_mpv, cmd) >= 0)
        emit seeked(seconds);
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

QString MpvObject::urlToLocalPath(const QUrl &url)
{
    return url.isLocalFile() ? url.toLocalFile() : url.toString();
}

// ---- subtitle / track control ----------------------------------------------
// Subtitle state and mpv calls live in SubtitleController; these are the
// QML-facing delegates (plus OSD announcements).

void MpvObject::selectAudioTrack(qint64 id)
{
    if (!m_mpv)
        return;

    const qint64 targetId = id <= 0 ? 0 : id;
    if (m_currentAudioTrack != targetId) {
        m_currentAudioTrack = targetId;
        for (QVariant &v : m_tracks) {
            QVariantMap t = v.toMap();
            if (t.value(QStringLiteral("type")).toString() == QLatin1String("audio")) {
                t[QStringLiteral("selected")] = (t.value(QStringLiteral("id")).toLongLong() == targetId);
                v = t;
            }
        }
        emit tracksChanged();
    }

    if (id <= 0) {
        mpv_set_property_string(m_mpv, "aid", "no");
        showOsd(QStringLiteral("Audio: Disabled"));
        return;
    }

    mpv_set_property(m_mpv, "aid", MPV_FORMAT_INT64, &id);

    QString label;
    for (const QVariant &v : m_tracks) {
        const QVariantMap t = v.toMap();
        if (t.value(QStringLiteral("type")).toString() == QLatin1String("audio") &&
            t.value(QStringLiteral("id")).toLongLong() == id) {
            const QString title = t.value(QStringLiteral("title")).toString();
            const QString lang = t.value(QStringLiteral("lang")).toString();
            if (title.isEmpty())
                label = lang.isEmpty() ? QStringLiteral("Track %1").arg(id)
                                       : QStringLiteral("%1").arg(lang);
            else if (!lang.isEmpty())
                label = QStringLiteral("%1 [%2]").arg(title, lang);
            else
                label = title;
            break;
        }
    }
    if (label.isEmpty())
        label = QStringLiteral("Track %1").arg(id);
    showOsd(QStringLiteral("Audio: %1").arg(label));
}

void MpvObject::selectSubtitleTrack(qint64 id)
{
    if (!m_mpv)
        return;

    const qint64 targetId = id <= 0 ? 0 : id;
    if (m_currentSubtitleTrack != targetId) {
        m_currentSubtitleTrack = targetId;
        for (QVariant &v : m_tracks) {
            QVariantMap t = v.toMap();
            if (t.value(QStringLiteral("type")).toString() == QLatin1String("sub")) {
                t[QStringLiteral("selected")] = (t.value(QStringLiteral("id")).toLongLong() == targetId);
                v = t;
            }
        }
        emit tracksChanged();
    }

    if (id <= 0) {
        m_subtitles.deselect();
        showOsd(QStringLiteral("Subtitles: Off"));
        return;
    }

    mpv_set_property(m_mpv, "sid", MPV_FORMAT_INT64, &id);
    if (!m_subtitles.visible())
        m_subtitles.setVisible(true);

    QString label;
    for (const QVariant &v : m_tracks) {
        const QVariantMap t = v.toMap();
        if (t.value(QStringLiteral("type")).toString() == QLatin1String("sub") &&
            t.value(QStringLiteral("id")).toLongLong() == id) {
            const QString title = t.value(QStringLiteral("title")).toString();
            const QString lang = t.value(QStringLiteral("lang")).toString();
            if (title.isEmpty())
                label = lang.isEmpty() ? QStringLiteral("Track %1").arg(id)
                                       : QStringLiteral("%1").arg(lang);
            else if (!lang.isEmpty())
                label = QStringLiteral("%1 [%2]").arg(title, lang);
            else
                label = title;
            break;
        }
    }
    if (label.isEmpty())
        label = QStringLiteral("Track %1").arg(id);
    showOsd(QStringLiteral("Subtitles: %1").arg(label));
}

void MpvObject::selectTrack(const QString &type, qint64 id)
{
    if (type == QLatin1String("audio"))
        selectAudioTrack(id);
    else if (type == QLatin1String("sub"))
        selectSubtitleTrack(id);
}

void MpvObject::selectTrack(qint64 id)
{
    if (!m_mpv)
        return;

    for (const QVariant &v : m_tracks) {
        const QVariantMap t = v.toMap();
        if (t.value(QStringLiteral("id")).toLongLong() != id)
            continue;
        const QString type = t.value(QStringLiteral("type")).toString();
        if (type == QLatin1String("sub")) {
            selectSubtitleTrack(id);
            return;
        } else if (type == QLatin1String("audio")) {
            selectAudioTrack(id);
            return;
        }
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

void MpvObject::setBrightness(double b)
{
    b = qBound(-100.0, b, 100.0);
    if (qFuzzyCompare(m_brightness, b))
        return;
    m_brightness = b;
    emit brightnessChanged();
    if (m_mpv)
        mpv_set_property_async(m_mpv, 0, "brightness", MPV_FORMAT_DOUBLE, &m_brightness);
}

void MpvObject::setContrast(double c)
{
    c = qBound(-100.0, c, 100.0);
    if (qFuzzyCompare(m_contrast, c))
        return;
    m_contrast = c;
    emit contrastChanged();
    if (m_mpv)
        mpv_set_property_async(m_mpv, 0, "contrast", MPV_FORMAT_DOUBLE, &m_contrast);
}

void MpvObject::setSaturation(double s)
{
    s = qBound(-100.0, s, 100.0);
    if (qFuzzyCompare(m_saturation, s))
        return;
    m_saturation = s;
    emit saturationChanged();
    if (m_mpv)
        mpv_set_property_async(m_mpv, 0, "saturation", MPV_FORMAT_DOUBLE, &m_saturation);
}

void MpvObject::setHue(double h)
{
    h = qBound(-100.0, h, 100.0);
    if (qFuzzyCompare(m_hue, h))
        return;
    m_hue = h;
    emit hueChanged();
    if (m_mpv)
        mpv_set_property_async(m_mpv, 0, "hue", MPV_FORMAT_DOUBLE, &m_hue);
}

void MpvObject::setGamma(double g)
{
    g = qBound(-100.0, g, 100.0);
    if (qFuzzyCompare(m_gamma, g))
        return;
    m_gamma = g;
    emit gammaChanged();
    if (m_mpv)
        mpv_set_property_async(m_mpv, 0, "gamma", MPV_FORMAT_DOUBLE, &m_gamma);
}

void MpvObject::setVideoRotate(int rot)
{
    rot = (rot % 360 + 360) % 360;
    if (m_videoRotate == rot)
        return;
    m_videoRotate = rot;
    emit videoRotateChanged();
    if (m_mpv) {
        int64_t r = m_videoRotate;
        mpv_set_property_async(m_mpv, 0, "video-rotate", MPV_FORMAT_INT64, &r);
        showOsd(QStringLiteral("Rotate: %1°").arg(m_videoRotate));
    }
}

void MpvObject::resetVideoAdjustments()
{
    setBrightness(0.0);
    setContrast(0.0);
    setSaturation(0.0);
    setHue(0.0);
    setGamma(0.0);
    showOsd(QStringLiteral("Video adjustments reset"));
}

void MpvObject::setDebandEnabled(bool enabled)
{
    if (m_debandEnabled == enabled)
        return;
    m_debandEnabled = enabled;
    emit debandChanged();
    if (m_mpv) {
        mpv_set_property_string(m_mpv, "deband", enabled ? "yes" : "no");
        showOsd(enabled ? QStringLiteral("Deband: ON") : QStringLiteral("Deband: OFF"));
    }
}

void MpvObject::setDebandIterations(int iter)
{
    iter = qBound(1, iter, 4);
    if (m_debandIterations == iter)
        return;
    m_debandIterations = iter;
    emit debandChanged();
    if (m_mpv) {
        int64_t v = iter;
        mpv_set_property_async(m_mpv, 0, "deband-iterations", MPV_FORMAT_INT64, &v);
    }
}

void MpvObject::setDebandThreshold(int thresh)
{
    thresh = qBound(16, thresh, 128);
    if (m_debandThreshold == thresh)
        return;
    m_debandThreshold = thresh;
    emit debandChanged();
    if (m_mpv) {
        int64_t v = thresh;
        mpv_set_property_async(m_mpv, 0, "deband-threshold", MPV_FORMAT_INT64, &v);
    }
}

void MpvObject::setDebandRange(int range)
{
    range = qBound(4, range, 32);
    if (m_debandRange == range)
        return;
    m_debandRange = range;
    emit debandChanged();
    if (m_mpv) {
        int64_t v = range;
        mpv_set_property_async(m_mpv, 0, "deband-range", MPV_FORMAT_INT64, &v);
    }
}

void MpvObject::toggleDeband()
{
    setDebandEnabled(!m_debandEnabled);
}

void MpvObject::setHdrToneMapping(const QString &algorithm)
{
    const QString algo = algorithm.trimmed().toLower();
    if (m_hdrToneMapping == algo)
        return;
    m_hdrToneMapping = algo;
    emit hdrToneMappingChanged();
    if (m_mpv) {
        const QByteArray bytes = algo.toUtf8();
        mpv_set_property_string(m_mpv, "tone-mapping", bytes.constData());
        showOsd(QStringLiteral("HDR Tone-Mapping: %1").arg(algo));
    }
}

void MpvObject::setHdrComputePeak(bool computePeak)
{
    if (m_hdrComputePeak == computePeak)
        return;
    m_hdrComputePeak = computePeak;
    emit hdrToneMappingChanged();
    if (m_mpv) {
        mpv_set_property_string(m_mpv, "hdr-compute-peak", computePeak ? "yes" : "no");
    }
}

void MpvObject::setVideoSharpen(double amount)
{
    amount = qBound(0.0, amount, 2.0);
    if (qFuzzyCompare(m_videoSharpen, amount))
        return;
    m_videoSharpen = amount;
    emit videoSharpenChanged();
    if (m_mpv) {
        double v = amount;
        mpv_set_property_async(m_mpv, 0, "sharpen", MPV_FORMAT_DOUBLE, &v);
        if (amount > 0.0)
            showOsd(QStringLiteral("Sharpen: %1").arg(QString::number(amount, 'f', 2)));
        else
            showOsd(QStringLiteral("Sharpen: OFF"));
    }
}

void MpvObject::toggleSharpen()
{
    if (m_videoSharpen > 0.0)
        setVideoSharpen(0.0);
    else
        setVideoSharpen(0.5);
}

void MpvObject::setEqualizerEnabled(bool enabled)
{
    if (m_equalizerEnabled == enabled)
        return;
    m_equalizerEnabled = enabled;
    emit equalizerEnabledChanged();
    updateEqualizerFilter();
    showOsd(enabled ? QStringLiteral("Equalizer: ON") : QStringLiteral("Equalizer: OFF"));
}

void MpvObject::setEqualizerBandGain(int bandIndex, double gainDb)
{
    if (bandIndex < 0 || bandIndex >= m_equalizerGains.size())
        return;
    gainDb = qBound(-12.0, gainDb, 12.0);
    if (qFuzzyCompare(m_equalizerGains[bandIndex].toDouble(), gainDb))
        return;
    m_equalizerGains[bandIndex] = gainDb;
    m_equalizerPreset = QStringLiteral("Custom");
    emit equalizerGainsChanged();
    emit equalizerPresetChanged();
    if (m_equalizerEnabled)
        updateEqualizerFilter();
}

void MpvObject::applyEqualizerPreset(const QString &presetName)
{
    QList<double> gains;
    if (presetName == QLatin1String("Rock"))
        gains = {4.5, 3.0, 1.5, 0.0, -1.0, -0.5, 1.5, 3.0, 4.0, 4.5};
    else if (presetName == QLatin1String("Pop"))
        gains = {-1.5, 1.0, 3.0, 3.5, 2.0, -0.5, -1.5, -1.5, 1.0, 2.5};
    else if (presetName == QLatin1String("Jazz"))
        gains = {3.0, 2.0, 1.0, 1.5, -1.5, -1.5, 0.0, 1.5, 2.5, 3.0};
    else if (presetName == QLatin1String("Classical"))
        gains = {4.0, 3.0, 2.5, 2.0, -1.5, -1.5, 0.0, 2.0, 3.0, 3.5};
    else if (presetName == QLatin1String("Bass Boost"))
        gains = {6.0, 5.0, 4.0, 2.5, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    else if (presetName == QLatin1String("Treble Boost"))
        gains = {0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 2.5, 4.0, 5.0, 6.0};
    else if (presetName == QLatin1String("Vocal") || presetName == QLatin1String("Speech"))
        gains = {-3.0, -2.0, -1.0, 1.5, 3.5, 3.5, 2.5, 1.0, -1.0, -2.0};
    else // Flat
        gains = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

    m_equalizerGains.clear();
    for (double g : gains)
        m_equalizerGains.append(g);
    m_equalizerPreset = presetName;

    emit equalizerGainsChanged();
    emit equalizerPresetChanged();

    if (m_equalizerEnabled)
        updateEqualizerFilter();
    showOsd(QStringLiteral("Equalizer: %1").arg(presetName));
}

void MpvObject::resetEqualizer()
{
    applyEqualizerPreset(QStringLiteral("Flat"));
}

void MpvObject::updateEqualizerFilter()
{
    if (!m_mpv)
        return;

    if (!m_equalizerEnabled) {
        const char *cmd[] = {"af", "remove", "@eq", nullptr};
        mpv_command_async(m_mpv, 0, cmd);
        return;
    }

    static const int freqs[10] = {31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
    QStringList bands;
    for (int i = 0; i < 10; ++i) {
        const double g = (i < m_equalizerGains.size()) ? m_equalizerGains[i].toDouble() : 0.0;
        bands << QStringLiteral("equalizer=f=%1:t=o:w=1:g=%2").arg(freqs[i]).arg(QString::number(g, 'f', 1));
    }
    const QString filterSpec = QStringLiteral("@eq:lavfi=[%1]").arg(bands.join(QLatin1Char(',')));
    const QByteArray filterUtf8 = filterSpec.toUtf8();
    const char *cmd[] = {"af", "add", filterUtf8.constData(), nullptr};
    mpv_command_async(m_mpv, 0, cmd);
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
    map.insert(QStringLiteral("performanceProfile"), m_performanceProfile);
    map.insert(QStringLiteral("isLowSpecActive"), m_lowSpecActive);

    const bool isSoftware = (hwdec.isEmpty() || hwdec == QLatin1String("no"));
    const bool is4k = (width >= 2560 || height >= 1440);
    QString hwWarning;
    if (is4k && isSoftware) {
        hwWarning = QStringLiteral("4K Software Decoding (High CPU - Lag Risk)");
    }
    map.insert(QStringLiteral("is4k"), is4k);
    map.insert(QStringLiteral("hwdecWarning"), hwWarning);
    map.insert(QStringLiteral("hwdecMode"), m_hwdecMode);

    return map;
}

void MpvObject::setHwdecMode(const QString &mode)
{
    const QString m = mode.toLower().trimmed();
    if (m_hwdecMode == m || !m_mpv)
        return;

    m_hwdecMode = m;
    mpv_set_property_string(m_mpv, "hwdec", m_hwdecMode.toUtf8().constData());
    emit hwdecModeChanged();
    applyPerformanceProfile();
    emit technicalStatsChanged();
}

void MpvObject::setPerformanceProfile(const QString &profile)
{
    const QString p = profile.toLower().trimmed();
    if (m_performanceProfile == p)
        return;

    m_performanceProfile = p;
    applyPerformanceProfile();
    emit performanceProfileChanged();
}

void MpvObject::applyPerformanceProfile()
{
    if (!m_mpv)
        return;

    int64_t w = 0, h = 0;
    mpv_get_property(m_mpv, "video-params/w", MPV_FORMAT_INT64, &w);
    mpv_get_property(m_mpv, "video-params/h", MPV_FORMAT_INT64, &h);
    const bool is4k = (w >= 2560 || h >= 1440);
    const bool hasVideoTrack = (w > 0 && h > 0);

    bool enableLowSpec = false;
    if (m_performanceProfile == QLatin1String("low-spec")) {
        enableLowSpec = true;
    } else if (m_performanceProfile == QLatin1String("high-quality")) {
        enableLowSpec = false;
    } else { // "auto"
        const bool isSoftware = (m_hwdec.isEmpty() || m_hwdec == QLatin1String("no"));
        enableLowSpec = (isSoftware && is4k);
    }

    // Dynamic adaptive demuxer buffer management:
    // Prevents buffer starvation on high-bitrate 4K files, while conserving memory on lower-tier hardware.
    if (is4k) {
        if (enableLowSpec) {
            mpv_set_property_string(m_mpv, "demuxer-max-bytes", "192MiB");
            mpv_set_property_string(m_mpv, "demuxer-max-back-bytes", "48MiB");
        } else {
            mpv_set_property_string(m_mpv, "demuxer-max-bytes", "256MiB");
            mpv_set_property_string(m_mpv, "demuxer-max-back-bytes", "64MiB");
        }
        mpv_set_property_string(m_mpv, "demuxer-readahead-secs", "20");
    } else if (hasVideoTrack) {
        if (enableLowSpec) {
            mpv_set_property_string(m_mpv, "demuxer-max-bytes", "48MiB");
            mpv_set_property_string(m_mpv, "demuxer-max-back-bytes", "16MiB");
            mpv_set_property_string(m_mpv, "demuxer-readahead-secs", "8");
        } else {
            mpv_set_property_string(m_mpv, "demuxer-max-bytes", "96MiB");
            mpv_set_property_string(m_mpv, "demuxer-max-back-bytes", "32MiB");
            mpv_set_property_string(m_mpv, "demuxer-readahead-secs", "12");
        }
    } else {
        mpv_set_property_string(m_mpv, "demuxer-max-bytes", "32MiB");
        mpv_set_property_string(m_mpv, "demuxer-max-back-bytes", "16MiB");
        mpv_set_property_string(m_mpv, "demuxer-readahead-secs", "5");
    }

    m_lowSpecActive = enableLowSpec;
    if (m_lowSpecActive) {
        // Low-spec / Software-fallback mode: Bilinear scaling, disable deband, fast decode
        mpv_set_property_string(m_mpv, "scale", "bilinear");
        mpv_set_property_string(m_mpv, "cscale", "bilinear");
        mpv_set_property_string(m_mpv, "dscale", "bilinear");
        mpv_set_property_string(m_mpv, "deband", "no");
        mpv_set_property_string(m_mpv, "vd-lavc-fast", "yes");
        mpv_set_property_string(m_mpv, "vd-lavc-skiploopfilter", "nonref");
    } else {
        // High-quality / Standard mode
        mpv_set_property_string(m_mpv, "scale", "spline36");
        mpv_set_property_string(m_mpv, "cscale", "spline36");
        mpv_set_property_string(m_mpv, "dscale", "mitchell");
        mpv_set_property_string(m_mpv, "deband", m_debandEnabled ? "yes" : "no");
        mpv_set_property_string(m_mpv, "vd-lavc-fast", "no");
        mpv_set_property_string(m_mpv, "vd-lavc-skiploopfilter", "default");
    }
    emit technicalStatsChanged();
}

bool MpvObject::copyScreenshotToClipboard()
{
    if (!hasVideo()) {
        showOsd(QStringLiteral("No video to capture"));
        return false;
    }

    bool ok = m_screenshot.copyToClipboard();
    if (ok) {
        showOsd(QStringLiteral("Frame copied to clipboard"));
    } else {
        showOsd(QStringLiteral("Capture failed"));
    }
    return ok;
}

QString MpvObject::saveScreenshot(const QString &folder)
{
    if (!hasVideo()) {
        showOsd(QStringLiteral("No video to capture"));
        return QString();
    }

    QString savedPath = m_screenshot.saveToFile(folder);
    if (!savedPath.isEmpty()) {
        const QString fileName = QFileInfo(savedPath).fileName();
        showOsd(QStringLiteral("Screenshot saved: %1").arg(fileName));
    } else {
        showOsd(QStringLiteral("Capture failed"));
    }
    return savedPath;
}

void MpvObject::setAbLoopA()
{
    if (!m_mpv)
        return;
    const double pos = m_position;
    const QByteArray posStr = QByteArray::number(pos, 'f', 3);
    mpv_set_property_string(m_mpv, "ab-loop-a", posStr.constData());
    m_abLoopA = pos;
    emit abLoopChanged();
    showOsd(QStringLiteral("A-B Loop: Point A (%1)").arg(formatTime(pos)));
}

void MpvObject::setAbLoopB()
{
    if (!m_mpv)
        return;
    const double pos = m_position;
    const QByteArray posStr = QByteArray::number(pos, 'f', 3);
    mpv_set_property_string(m_mpv, "ab-loop-b", posStr.constData());
    m_abLoopB = pos;
    emit abLoopChanged();
    showOsd(QStringLiteral("A-B Loop: Point B (%1)").arg(formatTime(pos)));
}

void MpvObject::clearAbLoop()
{
    if (!m_mpv)
        return;
    mpv_set_property_string(m_mpv, "ab-loop-a", "no");
    mpv_set_property_string(m_mpv, "ab-loop-b", "no");
    m_abLoopA = -1.0;
    m_abLoopB = -1.0;
    emit abLoopChanged();
    showOsd(QStringLiteral("A-B Loop: Cleared"));
}

void MpvObject::toggleAbLoop()
{
    if (m_abLoopA < 0.0) {
        setAbLoopA();
    } else if (m_abLoopB < 0.0) {
        setAbLoopB();
    } else {
        clearAbLoop();
    }
}

void MpvObject::autoSyncCurrentSubtitle()
{
    if (m_currentFile.isEmpty()) {
        showOsd(QStringLiteral("No media file playing"));
        return;
    }
    if (!m_subtitleSync.isToolAvailable()) {
        showOsd(QStringLiteral("ffsubsync not found (pip install ffsubsync)"));
        return;
    }

    // Check for sidecar .srt in same folder or cache
    QFileInfo fi(m_currentFile);
    QString srtCandidate = fi.absolutePath() + QStringLiteral("/") + fi.completeBaseName() + QStringLiteral(".srt");
    if (!QFile::exists(srtCandidate)) {
        showOsd(QStringLiteral("No sidecar .srt found to synchronize"));
        return;
    }

    showOsd(QStringLiteral("Auto-syncing subtitle with audio…"));
    m_subtitleSync.syncSubtitle(m_currentFile, srtCandidate);
}

bool MpvObject::hasYtdl() const
{
    return !QStandardPaths::findExecutable(QStringLiteral("yt-dlp")).isEmpty()
        || !QStandardPaths::findExecutable(QStringLiteral("youtube-dl")).isEmpty();
}

QString MpvObject::clipboardText() const
{
    QClipboard *cb = QGuiApplication::clipboard();
    return cb ? cb->text().trimmed() : QString();
}

QString MpvObject::normalizeStreamUrl(const QString &url)
{
    QString trimmed = url.trimmed();
    if (trimmed.isEmpty())
        return QString();

    // If scheme is missing, automatically add https://
    if (!trimmed.contains(QLatin1String("://"))) {
        if (trimmed.startsWith(QLatin1String("youtu.be/"), Qt::CaseInsensitive)
            || trimmed.startsWith(QLatin1String("youtube.com/"), Qt::CaseInsensitive)
            || trimmed.startsWith(QLatin1String("www.youtube.com/"), Qt::CaseInsensitive)
            || trimmed.startsWith(QLatin1String("m.youtube.com/"), Qt::CaseInsensitive)
            || trimmed.startsWith(QLatin1String("twitch.tv/"), Qt::CaseInsensitive)
            || trimmed.startsWith(QLatin1String("aparat.com/"), Qt::CaseInsensitive)
            || trimmed.startsWith(QLatin1String("vimeo.com/"), Qt::CaseInsensitive)
            || trimmed.startsWith(QLatin1String("www."), Qt::CaseInsensitive)) {
            trimmed.prepend(QLatin1String("https://"));
        }
    }
    return trimmed;
}

bool MpvObject::isValidStreamUrl(const QString &url) const
{
    const QString normalized = normalizeStreamUrl(url);
    if (normalized.isEmpty())
        return false;

    static const QStringList schemes = {
        QStringLiteral("http://"),
        QStringLiteral("https://"),
        QStringLiteral("rtmp://"),
        QStringLiteral("rtmps://"),
        QStringLiteral("rtsp://"),
        QStringLiteral("mms://"),
        QStringLiteral("magnet:"),
        QStringLiteral("udp://"),
        QStringLiteral("ytdl://")
    };
    for (const QString &scheme : schemes) {
        if (normalized.startsWith(scheme, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

void MpvObject::openUrl(const QString &url)
{
    const QString normalized = normalizeStreamUrl(url);
    if (normalized.isEmpty()) {
        showOsd(QStringLiteral("Invalid URL"));
        return;
    }

    const bool isStreamService = normalized.contains(QLatin1String("youtube.com"), Qt::CaseInsensitive)
                              || normalized.contains(QLatin1String("youtu.be"), Qt::CaseInsensitive)
                              || normalized.contains(QLatin1String("twitch.tv"), Qt::CaseInsensitive)
                              || normalized.contains(QLatin1String("aparat.com"), Qt::CaseInsensitive)
                              || normalized.contains(QLatin1String("vimeo.com"), Qt::CaseInsensitive);

    if (isStreamService && !hasYtdl()) {
        showOsd(QStringLiteral("yt-dlp is required: run 'sudo pacman -S yt-dlp'"));
        std::fprintf(stderr, "parvaz: yt-dlp is not installed; cannot resolve %s\n", qPrintable(normalized));
    }

    // Enable ytdl and optimized buffering for online streams
    if (m_mpv) {
        mpv_set_option_string(m_mpv, "ytdl", "yes");
        mpv_set_option_string(m_mpv, "demuxer-max-bytes", "64MiB");
        mpv_set_option_string(m_mpv, "demuxer-readahead-secs", "20");
    }

    if (!m_recentUrls.contains(normalized)) {
        m_recentUrls.prepend(normalized);
        if (m_recentUrls.size() > 10)
            m_recentUrls.removeLast();
        emit recentUrlsChanged();
    } else {
        m_recentUrls.removeAll(normalized);
        m_recentUrls.prepend(normalized);
        emit recentUrlsChanged();
    }

    loadFile(normalized);
    showOsd(QStringLiteral("Opening stream…"));
}

void MpvObject::clearRecentUrls()
{
    if (!m_recentUrls.isEmpty()) {
        m_recentUrls.clear();
        emit recentUrlsChanged();
    }
}
