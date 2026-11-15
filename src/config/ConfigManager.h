#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QFileSystemWatcher>

class MpvObject;

namespace parvaz {

class ConfigManager : public QObject {
    Q_OBJECT

    // Audio
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(bool audioNormalizer READ audioNormalizer WRITE setAudioNormalizer NOTIFY audioNormalizerChanged)

    // Video
    Q_PROPERTY(QString videoAspect READ videoAspect WRITE setVideoAspect NOTIFY videoAspectChanged)
    Q_PROPERTY(QString performanceProfile READ performanceProfile WRITE setPerformanceProfile NOTIFY performanceProfileChanged)

    // Subtitles
    Q_PROPERTY(double subtitleFontSize READ subtitleFontSize WRITE setSubtitleFontSize NOTIFY subtitleFontSizeChanged)
    Q_PROPERTY(double subtitlePosition READ subtitlePosition WRITE setSubtitlePosition NOTIFY subtitlePositionChanged)
    Q_PROPERTY(double subtitleDelay READ subtitleDelay WRITE setSubtitleDelay NOTIFY subtitleDelayChanged)

    // Window
    Q_PROPERTY(int windowWidth READ windowWidth WRITE setWindowWidth NOTIFY windowGeometryChanged)
    Q_PROPERTY(int windowHeight READ windowHeight WRITE setWindowHeight NOTIFY windowGeometryChanged)
    Q_PROPERTY(bool alwaysOnTop READ alwaysOnTop WRITE setAlwaysOnTop NOTIFY alwaysOnTopChanged)

public:
    explicit ConfigManager(MpvObject *mpv, const QString &explicitPath = QString(), QObject *parent = nullptr);
    ~ConfigManager() override;

    static ConfigManager *instance() { return s_instance; }

    QString configFilePath() const { return m_configPath; }

    int volume() const { return m_volume; }
    bool muted() const { return m_muted; }
    bool audioNormalizer() const { return m_audioNormalizer; }
    QString videoAspect() const { return m_videoAspect; }
    QString performanceProfile() const { return m_performanceProfile; }
    double subtitleFontSize() const { return m_subtitleFontSize; }
    double subtitlePosition() const { return m_subtitlePosition; }
    double subtitleDelay() const { return m_subtitleDelay; }
    int windowWidth() const { return m_windowWidth; }
    int windowHeight() const { return m_windowHeight; }
    bool alwaysOnTop() const { return m_alwaysOnTop; }

    Q_INVOKABLE void setVolume(int v);
    Q_INVOKABLE void setMuted(bool m);
    Q_INVOKABLE void setAudioNormalizer(bool n);
    Q_INVOKABLE void setVideoAspect(const QString &a);
    Q_INVOKABLE void setPerformanceProfile(const QString &p);
    Q_INVOKABLE void setSubtitleFontSize(double s);
    Q_INVOKABLE void setSubtitlePosition(double p);
    Q_INVOKABLE void setSubtitleDelay(double d);
    Q_INVOKABLE void setWindowSize(int w, int h);
    Q_INVOKABLE void setWindowWidth(int w);
    Q_INVOKABLE void setWindowHeight(int h);
    Q_INVOKABLE void setAlwaysOnTop(bool onTop);

    Q_INVOKABLE void reload();
    Q_INVOKABLE void saveNow();

signals:
    void volumeChanged();
    void mutedChanged();
    void audioNormalizerChanged();
    void videoAspectChanged();
    void performanceProfileChanged();
    void subtitleFontSizeChanged();
    void subtitlePositionChanged();
    void subtitleDelayChanged();
    void windowGeometryChanged();
    void alwaysOnTopChanged();
    void configReloaded();

private slots:
    void onFileChanged(const QString &path);
    void writeToDisk();

private:
    void load();
    void applyToPlayer();
    void scheduleSave();

    static ConfigManager *s_instance;
    MpvObject *m_mpv = nullptr;
    QString m_configPath;
    QTimer m_saveDebounceTimer;
    QFileSystemWatcher m_watcher;

    bool m_loading = false;
    bool m_applyingToPlayer = false;

    // Config Values (with defaults)
    int m_volume = 100;
    bool m_muted = false;
    bool m_audioNormalizer = false;
    QString m_videoAspect = QStringLiteral("default");
    QString m_performanceProfile = QStringLiteral("auto");
    double m_subtitleFontSize = 38.0;
    double m_subtitlePosition = 100.0;
    double m_subtitleDelay = 0.0;
    int m_windowWidth = 1280;
    int m_windowHeight = 720;
    bool m_alwaysOnTop = false;
};

} // namespace parvaz
