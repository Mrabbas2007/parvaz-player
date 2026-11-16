#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QVariantList>
#include <atomic>

#include <mpv/client.h>
#include <mpv/SubtitleController.h>
#include <playback/CoverArtProvider.h>
#include <playback/ScreenshotController.h>
#include <subs/SubtitleSearchService.h>
#include <subs/SubtitleSyncService.h>

namespace parvaz { class PlaybackHistory; }

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
    /// Pre-filtered track lists for menus. Bindings must NOT call
    /// tracksOfType(): a Q_INVOKABLE's internals are invisible to the QML
    /// dependency tracker, so the binding would evaluate once at startup
    /// (empty) and never refresh. Properties re-evaluate on tracksChanged.
    Q_PROPERTY(QVariantList audioTracks READ audioTracks NOTIFY tracksChanged)
    Q_PROPERTY(QVariantList subTracks READ subTracks NOTIFY tracksChanged)
    Q_PROPERTY(qint64 currentAudioTrack READ currentAudioTrack NOTIFY tracksChanged)
    Q_PROPERTY(qint64 currentSubtitleTrack READ currentSubtitleTrack NOTIFY tracksChanged)
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
    Q_PROPERTY(QVariantList chapters READ chapters NOTIFY chaptersChanged)
    Q_PROPERTY(int currentChapter READ currentChapter NOTIFY currentChapterChanged)
    Q_PROPERTY(QVariantList bookmarks READ bookmarks NOTIFY bookmarksChanged)
    Q_PROPERTY(bool audioNormalizer READ audioNormalizer WRITE setAudioNormalizer NOTIFY audioNormalizerChanged)
    Q_PROPERTY(QString performanceProfile READ performanceProfile WRITE setPerformanceProfile NOTIFY performanceProfileChanged)
    Q_PROPERTY(qint64 secondarySubtitleTrack READ secondarySubtitleTrack NOTIFY secondarySubtitleTrackChanged)
    Q_PROPERTY(bool isAudioOnly READ isAudioOnly NOTIFY isAudioOnlyChanged)
    Q_PROPERTY(QVariantMap metadata READ metadata NOTIFY metadataChanged)
    Q_PROPERTY(QString trackTitle READ trackTitle NOTIFY metadataChanged)
    Q_PROPERTY(QString trackArtist READ trackArtist NOTIFY metadataChanged)
    Q_PROPERTY(QString trackAlbum READ trackAlbum NOTIFY metadataChanged)
    Q_PROPERTY(QString trackYear READ trackYear NOTIFY metadataChanged)
    Q_PROPERTY(QString trackGenre READ trackGenre NOTIFY metadataChanged)
    Q_PROPERTY(QString coverArtUrl READ coverArtUrl NOTIFY coverArtUrlChanged)
    Q_PROPERTY(double abLoopA READ abLoopA NOTIFY abLoopChanged)
    Q_PROPERTY(double abLoopB READ abLoopB NOTIFY abLoopChanged)
    Q_PROPERTY(bool abLoopActive READ abLoopActive NOTIFY abLoopChanged)

    // Video Adjustments
    Q_PROPERTY(double brightness READ brightness WRITE setBrightness NOTIFY brightnessChanged)
    Q_PROPERTY(double contrast READ contrast WRITE setContrast NOTIFY contrastChanged)
    Q_PROPERTY(double saturation READ saturation WRITE setSaturation NOTIFY saturationChanged)
    Q_PROPERTY(double hue READ hue WRITE setHue NOTIFY hueChanged)
    Q_PROPERTY(double gamma READ gamma WRITE setGamma NOTIFY gammaChanged)
    Q_PROPERTY(int videoRotate READ videoRotate WRITE setVideoRotate NOTIFY videoRotateChanged)

    // Audio Equalizer (10-Band)
    Q_PROPERTY(bool equalizerEnabled READ equalizerEnabled WRITE setEqualizerEnabled NOTIFY equalizerEnabledChanged)
    Q_PROPERTY(QString equalizerPreset READ equalizerPreset NOTIFY equalizerPresetChanged)
    Q_PROPERTY(QVariantList equalizerGains READ equalizerGains NOTIFY equalizerGainsChanged)

    // Online Subtitle Services (Search & Auto-Sync)
    Q_PROPERTY(SubtitleSearchService* subtitleSearch READ subtitleSearch CONSTANT)
    Q_PROPERTY(SubtitleSyncService* subtitleSync READ subtitleSync CONSTANT)

    // Online Streaming & URLs (Ctrl+U)
    Q_PROPERTY(QStringList recentUrls READ recentUrls NOTIFY recentUrlsChanged)
    Q_PROPERTY(bool hasYtdl READ hasYtdl CONSTANT)

    // Loading & Buffering State (for UX spinner / network & 4K media loading)
    Q_PROPERTY(bool loading READ isLoading NOTIFY loadingChanged)
    Q_PROPERTY(bool buffering READ isBuffering NOTIFY bufferingChanged)
    Q_PROPERTY(int bufferPercent READ bufferPercent NOTIFY bufferPercentChanged)
    Q_PROPERTY(bool eofReached READ eofReached NOTIFY eofReachedChanged)

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
    bool isAudioOnly() const;
    QVariantMap metadata() const { return m_metadata; }
    QString trackTitle() const;
    QString trackArtist() const;
    QString trackAlbum() const;
    QString trackYear() const;
    QString trackGenre() const;
    QString coverArtUrl() const { return m_coverArtUrl; }
    QVariantList tracks() const { return m_tracks; }
    QVariantList audioTracks() const { return tracksOfType(QStringLiteral("audio")); }
    QVariantList subTracks() const { return tracksOfType(QStringLiteral("sub")); }
    qint64 currentAudioTrack() const { return m_currentAudioTrack; }
    qint64 currentSubtitleTrack() const { return m_currentSubtitleTrack; }
    double subtitleDelay() const { return m_subtitles.delay(); }
    bool subtitlesVisible() const { return m_subtitles.visible(); }
    double subtitlePosition() const { return m_subtitles.position(); }
    double subtitleFontSize() const { return m_subtitles.fontSize(); }
    QString videoAspect() const { return m_videoAspect; }
    int playlistCount() const { return m_playlistCount; }
    int playlistIndex() const { return m_playlistIndex; }
    bool hasNext() const { return m_playlistCount > 0 && m_playlistIndex >= 0 && m_playlistIndex < m_playlistCount - 1; }
    bool hasPrevious() const { return m_playlistCount > 0 && m_playlistIndex > 0; }
    QVariantList playlist() const { return m_playlist; }
    QVariantList chapters() const { return m_chapters; }
    int currentChapter() const { return m_currentChapter; }
    QVariantList bookmarks() const { return m_bookmarks; }
    bool audioNormalizer() const { return m_audioNormalizer; }
    QString performanceProfile() const { return m_performanceProfile; }
    qint64 secondarySubtitleTrack() const { return m_subtitles.secondaryTrack(); }
    double abLoopA() const { return m_abLoopA; }
    double abLoopB() const { return m_abLoopB; }
    bool abLoopActive() const { return m_abLoopA >= 0.0 && m_abLoopB > m_abLoopA; }

    double brightness() const { return m_brightness; }
    double contrast() const { return m_contrast; }
    double saturation() const { return m_saturation; }
    double hue() const { return m_hue; }
    double gamma() const { return m_gamma; }
    int videoRotate() const { return m_videoRotate; }

    bool equalizerEnabled() const { return m_equalizerEnabled; }
    QString equalizerPreset() const { return m_equalizerPreset; }
    QVariantList equalizerGains() const { return m_equalizerGains; }

    SubtitleSearchService *subtitleSearch() { return &m_subtitleSearch; }
    SubtitleSyncService *subtitleSync() { return &m_subtitleSync; }
    Q_INVOKABLE void autoSyncCurrentSubtitle();

    /// Online Streaming & URL Handling
    QStringList recentUrls() const { return m_recentUrls; }
    bool hasYtdl() const;
    Q_INVOKABLE QString clipboardText() const;
    Q_INVOKABLE static QString normalizeStreamUrl(const QString &url);
    Q_INVOKABLE bool isValidStreamUrl(const QString &url) const;
    Q_INVOKABLE void openUrl(const QString &url);
    Q_INVOKABLE void clearRecentUrls();

    /// Loading & Buffering State
    bool isLoading() const { return m_isLoading; }
    bool isBuffering() const { return m_isLoading || m_pausedForCache || (m_seeking && m_coreIdle); }
    int bufferPercent() const { return m_bufferPercent; }
    bool eofReached() const { return m_eofReached; }

    void setHistory(parvaz::PlaybackHistory *history);
    void setAudioNormalizer(bool enabled);
    Q_INVOKABLE void setPerformanceProfile(const QString &profile);

    // Q_PROPERTY WRITE setters, also Q_INVOKABLE so QML can call them as
    // functions (Mpv.setMuted(...)). Plain WRITE accessors are NOT callable
    // from QML — omitting Q_INVOKABLE made every such call a silent
    // TypeError and the action never ran (setVideoAspect set the precedent).
    Q_INVOKABLE void setPaused(bool paused);
    Q_INVOKABLE void setVolume(int volume);
    Q_INVOKABLE void setMuted(bool muted);
    Q_INVOKABLE void setSpeed(double speed);
    Q_INVOKABLE void setSubtitleDelay(double seconds);
    Q_INVOKABLE void setSubtitlesVisible(bool visible);
    Q_INVOKABLE void setSubtitlePosition(double percentFromTop);
    Q_INVOKABLE void setSubtitleFontSize(double size);
    Q_INVOKABLE void setVideoAspect(const QString &aspect);

    // Video adjustments
    Q_INVOKABLE void setBrightness(double b);
    Q_INVOKABLE void setContrast(double c);
    Q_INVOKABLE void setSaturation(double s);
    Q_INVOKABLE void setHue(double h);
    Q_INVOKABLE void setGamma(double g);
    Q_INVOKABLE void setVideoRotate(int rot);
    Q_INVOKABLE void resetVideoAdjustments();

    // Audio Equalizer
    Q_INVOKABLE void setEqualizerEnabled(bool enabled);
    Q_INVOKABLE void setEqualizerBandGain(int bandIndex, double gainDb);
    Q_INVOKABLE void applyEqualizerPreset(const QString &presetName);
    Q_INVOKABLE void resetEqualizer();

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

    /// file:// URL (or plain path) → local filesystem path; non-file URLs
    /// pass through unchanged. Single source of truth for the file dialogs
    /// and the drop overlay (used to be duplicated JS in QML).
    Q_INVOKABLE static QString urlToLocalPath(const QUrl &url);

    /// Selects an audio ("aid") track by track id (or <= 0 for "no" / disable).
    Q_INVOKABLE void selectAudioTrack(qint64 id);

    /// Selects a subtitle ("sid") track by track id (or <= 0 for "no" / disable).
    Q_INVOKABLE void selectSubtitleTrack(qint64 id);

    /// Selects a track by explicit type ("audio", "sub") and id.
    Q_INVOKABLE void selectTrack(const QString &type, qint64 id);

    /// Legacy fallback: selects track by id. Prefer selectAudioTrack/selectSubtitleTrack.
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
    Q_INVOKABLE void selectSecondarySubtitleTrack(qint64 id);
    Q_INVOKABLE void cycleSecondarySubtitleTrack();

    /// Chapters
    Q_INVOKABLE void nextChapter();
    Q_INVOKABLE void previousChapter();
    Q_INVOKABLE void setChapter(int index);

    /// Bookmarks
    Q_INVOKABLE void addBookmark(const QString &title = QString());
    Q_INVOKABLE void removeBookmark(int id);
    Q_INVOKABLE void clearBookmarks();
    Q_INVOKABLE void refreshBookmarks();

    /// Audio normalizer (Night mode)
    Q_INVOKABLE void toggleAudioNormalizer();

    /// Technical specs (for OSD with Tab key)
    Q_INVOKABLE QVariantMap technicalStats() const;

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

    /// A-B Repeat Loop
    Q_INVOKABLE void setAbLoopA();
    Q_INVOKABLE void setAbLoopB();
    Q_INVOKABLE void clearAbLoop();
    Q_INVOKABLE void toggleAbLoop();

    /// Screenshots / Frame Capture
    Q_INVOKABLE bool copyScreenshotToClipboard();
    Q_INVOKABLE QString saveScreenshot(const QString &folder = QString());

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
    void chaptersChanged();
    void currentChapterChanged();
    void bookmarksChanged();
    void audioNormalizerChanged();
    void performanceProfileChanged();
    void abLoopChanged();
    void brightnessChanged();
    void contrastChanged();
    void saturationChanged();
    void hueChanged();
    void gammaChanged();
    void videoRotateChanged();
    void equalizerEnabledChanged();
    void equalizerPresetChanged();
    void equalizerGainsChanged();
    void recentUrlsChanged();
    void secondarySubtitleTrackChanged();
    void technicalStatsChanged();
    void isAudioOnlyChanged();
    void loadingChanged();
    void bufferingChanged();
    void bufferPercentChanged();
    void eofReachedChanged();
    void metadataChanged();
    void coverArtUrlChanged();
    void fileLoaded();
    void osdRequested(const QString &text);
    /// Emitted when playback jumps abruptly (user seek or resume). MPRIS
    /// clients need this to resync their position display ("Seeked").
    void seeked(double position);

private slots:
    void onWakeup();

private:
    void drainEvents();
    void refreshTracks();
    void refreshPlaylist();
    void refreshChapters();
    void refreshMetadata();
    void refreshCoverArt();
    void setCoverArtUrl(const QString &url);
    void applyPerformanceProfile();
    void updateEqualizerFilter();
    QStringList expandAndSortPaths(const QStringList &rawPaths);
    static void wakeupCallback(void *ctx);

    static MpvObject *s_instance;
    mpv_handle *m_mpv = nullptr;
    parvaz::PlaybackHistory *m_history = nullptr;
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

    QVariantList m_chapters;
    int m_currentChapter = -1;
    QVariantList m_bookmarks;
    bool m_audioNormalizer = false;
    QString m_performanceProfile = QStringLiteral("auto"); // "auto", "low-spec", "high-quality"
    bool m_lowSpecActive = false;
    double m_abLoopA = -1.0;
    double m_abLoopB = -1.0;
    QVariantMap m_metadata;
    QString m_coverArtUrl;
    bool m_isAudioOnly = false;
    /// Cover-art lookup and extraction (see playback/CoverArtProvider.h).
    parvaz::CoverArtProvider m_coverArt;

    /// Subtitle state and mpv wiring; QML-facing API stays on MpvObject.
    SubtitleController m_subtitles;

    /// Screenshot & frame capture controller
    parvaz::ScreenshotController m_screenshot;

    /// Subtitle online search and synchronization services
    SubtitleSearchService m_subtitleSearch;
    SubtitleSyncService m_subtitleSync;

    QVariantList m_tracks;
    qint64 m_currentAudioTrack = 0;
    qint64 m_currentSubtitleTrack = 0;
    QString m_videoAspect = QStringLiteral("default");

    // Video adjustments
    double m_brightness = 0.0;
    double m_contrast = 0.0;
    double m_saturation = 0.0;
    double m_hue = 0.0;
    double m_gamma = 0.0;
    int m_videoRotate = 0;

    // Audio Equalizer
    bool m_equalizerEnabled = false;
    QString m_equalizerPreset = QStringLiteral("Flat");
    QVariantList m_equalizerGains = {QVariant(0.0), QVariant(0.0), QVariant(0.0), QVariant(0.0), QVariant(0.0),
                                     QVariant(0.0), QVariant(0.0), QVariant(0.0), QVariant(0.0), QVariant(0.0)};

    QStringList m_recentUrls;

    // Loading & Buffering state
    bool m_isLoading = false;
    bool m_pausedForCache = false;
    bool m_seeking = false;
    bool m_coreIdle = false;
    int m_bufferPercent = 100;
    bool m_eofReached = false;
};
