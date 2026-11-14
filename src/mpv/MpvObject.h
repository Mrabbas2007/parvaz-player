#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QVariantList>
#include <atomic>

#include <mpv/client.h>

/// Thin, GUI-thread-owned wrapper around mpv_handle.
/// All client-API calls happen on the Qt main thread.
/// The render context lives in MpvFboItem's renderer (render thread).
class MpvObject : public QObject {
    Q_OBJECT
    Q_PROPERTY(double position READ position NOTIFY positionChanged)
    Q_PROPERTY(double duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(double speed READ speed WRITE setSpeed NOTIFY speedChanged)
    Q_PROPERTY(QString mediaTitle READ mediaTitle NOTIFY mediaTitleChanged)
    Q_PROPERTY(QString currentFile READ currentFile NOTIFY currentFileChanged)
    Q_PROPERTY(QString hwdec READ hwdec NOTIFY hwdecChanged)
    Q_PROPERTY(bool hasVideo READ hasVideo NOTIFY hasVideoChanged)
    /// One QVariantMap per track: id, type, title, lang, codec, external,
    /// selected, default, forced. Derived from mpv's "track-list".
    Q_PROPERTY(QVariantList tracks READ tracks NOTIFY tracksChanged)
    Q_PROPERTY(double subtitleDelay READ subtitleDelay WRITE setSubtitleDelay
                   NOTIFY subtitleDelayChanged)
    Q_PROPERTY(bool subtitlesVisible READ subtitlesVisible WRITE setSubtitlesVisible
                   NOTIFY subtitlesVisibleChanged)
    /// Float option, 0..150, percent from the top (mpv default 100).
    Q_PROPERTY(double subtitlePosition READ subtitlePosition WRITE setSubtitlePosition
                   NOTIFY subtitlePositionChanged)
    /// Float option in scaled pixels at 720p (mpv default 38).
    Q_PROPERTY(double subtitleFontSize READ subtitleFontSize WRITE setSubtitleFontSize
                   NOTIFY subtitleFontSizeChanged)
    Q_PROPERTY(QString videoAspect READ videoAspect WRITE setVideoAspect NOTIFY videoAspectChanged)
    Q_PROPERTY(int playlistCount READ playlistCount NOTIFY playlistChanged)
    Q_PROPERTY(int playlistIndex READ playlistIndex NOTIFY playlistChanged)
    Q_PROPERTY(bool hasNext READ hasNext NOTIFY playlistChanged)
    Q_PROPERTY(bool hasPrevious READ hasPrevious NOTIFY playlistChanged)
    Q_PROPERTY(QVariantList playlist READ playlist NOTIFY playlistChanged)

public:
    explicit MpvObject(QObject *parent = nullptr);
    ~MpvObject() override;

    static MpvObject *instance() { return s_instance; }
    mpv_handle *handle() const { return m_mpv; }

    double position() const { return m_position; }
    double duration() const { return m_duration; }
    bool paused() const { return m_paused; }
    int volume() const { return m_volume; }
    bool muted() const { return m_muted; }
    double speed() const { return m_speed; }
    QString mediaTitle() const { return m_mediaTitle; }
    QString currentFile() const { return m_currentFile; }
    QString hwdec() const { return m_hwdec; }
    bool hasVideo() const;
    QVariantList tracks() const { return m_tracks; }
    double subtitleDelay() const { return m_subtitleDelay; }
    bool subtitlesVisible() const { return m_subtitlesVisible; }
    double subtitlePosition() const { return m_subtitlePosition; }
    double subtitleFontSize() const { return m_subtitleFontSize; }
    QString videoAspect() const { return m_videoAspect; }
    int playlistCount() const { return m_playlistCount; }
    int playlistIndex() const { return m_playlistIndex; }
    bool hasNext() const { return m_playlistCount > 0 && m_playlistIndex >= 0 && m_playlistIndex < m_playlistCount - 1; }
    bool hasPrevious() const { return m_playlistCount > 0 && m_playlistIndex > 0; }
    QVariantList playlist() const { return m_playlist; }

    void setPaused(bool paused);
    void setVolume(int volume);
    void setMuted(bool muted);
    void setSpeed(double speed);
    void setSubtitleDelay(double seconds);
    void setSubtitlesVisible(bool visible);
    void setSubtitlePosition(double percentFromTop);
    void setSubtitleFontSize(double size);
    Q_INVOKABLE void setVideoAspect(const QString &aspect);

    /// mpv accepts --speed 0.01..100 (see `mpv --speed=` docs).
    static constexpr double kMinSpeed = 0.01;
    static constexpr double kMaxSpeed = 100.0;

    Q_INVOKABLE void loadFile(const QString &path);
    Q_INVOKABLE void loadFiles(const QStringList &paths, const QString &mode = QStringLiteral("replace"));
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void playIndex(int index);
    Q_INVOKABLE void removeIndex(int index);
    Q_INVOKABLE void clearPlaylist();
    Q_INVOKABLE void shufflePlaylist();

    /// Called (marshalled) from the render thread once the mpv render context
    /// exists. Playback must not start before that, otherwise mpv fails to
    /// initialise the video output ("vo=libmpv" with no consumer).
    void setRenderReady();
    Q_INVOKABLE void togglePause();
    Q_INVOKABLE void seekRelative(double seconds);

    /// Absolute seek, used by the seekbar.
    Q_INVOKABLE void seekAbsolute(double seconds);

    /// One frame forwards or backwards. Works while paused: mpv steps by
    /// playing frames and re-pausing ("play" mode) or by seeking (-1).
    Q_INVOKABLE void frameStep(int n = 1);

    Q_INVOKABLE QString formatTime(double seconds) const;

    /// Selects an audio ("aid") or subtitle ("sid") track by its track-list id.
    /// The id alone is ambiguous, so the type is looked up in the track list.
    Q_INVOKABLE void selectTrack(qint64 id);

    /// Deselects subtitles ("sid" = "no"). The track stays loaded.
    Q_INVOKABLE void disableSubtitles();

    /// Loads an external subtitle file and selects it ("select" flag).
    Q_INVOKABLE void addSubtitleFile(const QString &path);

    /// "sub-codepage" forces a legacy encoding, then reloads the current
    /// subtitle so it is re-decoded. libass/HarfBuzz shape Persian correctly
    /// once the bytes are right; we never reverse text by hand.
    Q_INVOKABLE void setSubtitleCodepage(const QString &codepage);

    /// Current "sub-codepage" option, for assertions on the setter.
    Q_INVOKABLE QString subtitleCodepage() const;

    /// Re-runs the current external subtitle decode (sub-reload).
    Q_INVOKABLE void reloadSubtitles();

    /// Text of the subtitle on screen right now, empty when nothing shows.
    /// Synchronous on purpose: the smoke test asserts on the return value.
    Q_INVOKABLE QString subtitleText() const;

    Q_INVOKABLE void cycleSubtitleTrack();
    Q_INVOKABLE void cycleAudioTrack();

    /// The tracks of one type ("audio" / "video" / "sub"), each reduced to
    /// { id, label, selected } so QML needs no filtering logic.
    Q_INVOKABLE QVariantList tracksOfType(const QString &type) const;

    /// Updates the bottom control bar offset so subtitles shift above the bar
    /// whenever the bar is visible on screen.
    Q_INVOKABLE void setBottomBarOffset(bool visible, double barHeight, double windowHeight);

    /// Shortcuts and OSD helpers
    Q_INVOKABLE void adjustSpeed(double delta);
    Q_INVOKABLE void resetSpeed();
    Q_INVOKABLE void adjustVolume(int delta);
    Q_INVOKABLE void adjustSubtitlePosition(double delta);
    Q_INVOKABLE void adjustSubtitleFontSize(double delta);
    Q_INVOKABLE void showOsd(const QString &text);

    /// "hwdec=vaapi drop=0 decoder-drop=0" for benchmarks.
    QString statsLine() const;

signals:
    void positionChanged();
    void durationChanged();
    void pausedChanged();
    void volumeChanged();
    void mutedChanged();
    void speedChanged();
    void mediaTitleChanged();
    void currentFileChanged();
    void hwdecChanged();
    void hasVideoChanged();
    void tracksChanged();
    void subtitleDelayChanged();
    void subtitlesVisibleChanged();
    void subtitlePositionChanged();
    void subtitleFontSizeChanged();
    void videoAspectChanged();
    void playlistChanged();
    void fileLoaded();
    void osdRequested(const QString &text);

private slots:
    void onWakeup();

private:
    void drainEvents();
    void refreshTracks();
    void refreshPlaylist();
    QStringList expandAndSortPaths(const QStringList &rawPaths);
    void applySubtitlePosition();
    static void wakeupCallback(void *ctx);

    static MpvObject *s_instance;
    mpv_handle *m_mpv = nullptr;
    std::atomic_bool m_wakeupPending{false};

    bool m_renderReady = false;
    QStringList m_pendingFiles;
    QString m_pendingMode;

    double m_position = 0.0;
    double m_duration = 0.0;
    bool m_paused = true;
    int m_volume = 100;
    bool m_muted = false;
    double m_speed = 1.0;
    QString m_mediaTitle;
    QString m_currentFile;
    QString m_hwdec;

    int m_playlistCount = 0;
    int m_playlistIndex = -1;
    QVariantList m_playlist;

    QVariantList m_tracks;
    double m_subtitleDelay = 0.0;
    bool m_subtitlesVisible = true;
    double m_subtitlePosition = 100.0;
    double m_subtitleFontSize = 38.0;
    QString m_videoAspect = QStringLiteral("default");

    bool m_bottomBarVisible = false;
    double m_bottomBarHeight = 0.0;
    double m_windowHeight = 0.0;
    double m_lastAppliedSubPos = 100.0;
};
