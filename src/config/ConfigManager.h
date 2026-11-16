#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QFileSystemWatcher>
#include <QVariant>

class MpvObject;

namespace parvaz {

class ConfigManager : public QObject {
    Q_OBJECT

    // Audio
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(bool audioNormalizer READ audioNormalizer WRITE setAudioNormalizer NOTIFY audioNormalizerChanged)
    Q_PROPERTY(bool equalizerEnabled READ equalizerEnabled WRITE setEqualizerEnabled NOTIFY equalizerChanged)
    Q_PROPERTY(QString equalizerPreset READ equalizerPreset WRITE setEqualizerPreset NOTIFY equalizerChanged)
    Q_PROPERTY(QVariantList equalizerGains READ equalizerGains WRITE setEqualizerGains NOTIFY equalizerChanged)

    // Video
    Q_PROPERTY(QString videoAspect READ videoAspect WRITE setVideoAspect NOTIFY videoAspectChanged)
    Q_PROPERTY(QString performanceProfile READ performanceProfile WRITE setPerformanceProfile NOTIFY performanceProfileChanged)
    Q_PROPERTY(QString hwdecMode READ hwdecMode WRITE setHwdecMode NOTIFY hwdecModeChanged)
    Q_PROPERTY(double brightness READ brightness WRITE setBrightness NOTIFY videoAdjustmentsChanged)
    Q_PROPERTY(double contrast READ contrast WRITE setContrast NOTIFY videoAdjustmentsChanged)
    Q_PROPERTY(double saturation READ saturation WRITE setSaturation NOTIFY videoAdjustmentsChanged)
    Q_PROPERTY(double hue READ hue WRITE setHue NOTIFY videoAdjustmentsChanged)
    Q_PROPERTY(double gamma READ gamma WRITE setGamma NOTIFY videoAdjustmentsChanged)

    // Video Processing & HDR
    Q_PROPERTY(bool debandEnabled READ debandEnabled WRITE setDebandEnabled NOTIFY debandChanged)
    Q_PROPERTY(int debandIterations READ debandIterations WRITE setDebandIterations NOTIFY debandChanged)
    Q_PROPERTY(int debandThreshold READ debandThreshold WRITE setDebandThreshold NOTIFY debandChanged)
    Q_PROPERTY(int debandRange READ debandRange WRITE setDebandRange NOTIFY debandChanged)
    Q_PROPERTY(QString hdrToneMapping READ hdrToneMapping WRITE setHdrToneMapping NOTIFY hdrToneMappingChanged)
    Q_PROPERTY(bool hdrComputePeak READ hdrComputePeak WRITE setHdrComputePeak NOTIFY hdrToneMappingChanged)
    Q_PROPERTY(double videoSharpen READ videoSharpen WRITE setVideoSharpen NOTIFY videoSharpenChanged)

    // Appearance & Keymap
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(QString keymapPreset READ keymapPreset WRITE setKeymapPreset NOTIFY keymapPresetChanged)

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
    QString hwdecMode() const { return m_hwdecMode; }
    double subtitleFontSize() const { return m_subtitleFontSize; }
    double subtitlePosition() const { return m_subtitlePosition; }
    double subtitleDelay() const { return m_subtitleDelay; }
    int windowWidth() const { return m_windowWidth; }
    int windowHeight() const { return m_windowHeight; }
    bool alwaysOnTop() const { return m_alwaysOnTop; }

    double brightness() const { return m_brightness; }
    double contrast() const { return m_contrast; }
    double saturation() const { return m_saturation; }
    double hue() const { return m_hue; }
    double gamma() const { return m_gamma; }

    bool debandEnabled() const { return m_debandEnabled; }
    int debandIterations() const { return m_debandIterations; }
    int debandThreshold() const { return m_debandThreshold; }
    int debandRange() const { return m_debandRange; }
    QString hdrToneMapping() const { return m_hdrToneMapping; }
    bool hdrComputePeak() const { return m_hdrComputePeak; }
    double videoSharpen() const { return m_videoSharpen; }
    QString theme() const { return m_theme; }
    QString keymapPreset() const { return m_keymapPreset; }

    bool equalizerEnabled() const { return m_equalizerEnabled; }
    QString equalizerPreset() const { return m_equalizerPreset; }
    QVariantList equalizerGains() const { return m_equalizerGains; }

    Q_INVOKABLE void setVolume(int v);
    Q_INVOKABLE void setMuted(bool m);
    Q_INVOKABLE void setAudioNormalizer(bool n);
    Q_INVOKABLE void setEqualizerEnabled(bool e);
    Q_INVOKABLE void setEqualizerPreset(const QString &p);
    Q_INVOKABLE void setEqualizerGains(const QVariantList &gains);

    Q_INVOKABLE void setVideoAspect(const QString &a);
    Q_INVOKABLE void setPerformanceProfile(const QString &p);
    Q_INVOKABLE void setHwdecMode(const QString &m);
    Q_INVOKABLE void setBrightness(double b);
    Q_INVOKABLE void setContrast(double c);
    Q_INVOKABLE void setSaturation(double s);
    Q_INVOKABLE void setHue(double h);
    Q_INVOKABLE void setGamma(double g);

    Q_INVOKABLE void setDebandEnabled(bool enabled);
    Q_INVOKABLE void setDebandIterations(int iter);
    Q_INVOKABLE void setDebandThreshold(int thresh);
    Q_INVOKABLE void setDebandRange(int range);
    Q_INVOKABLE void setHdrToneMapping(const QString &algorithm);
    Q_INVOKABLE void setHdrComputePeak(bool computePeak);
    Q_INVOKABLE void setVideoSharpen(double amount);
    Q_INVOKABLE void setTheme(const QString &theme);
    Q_INVOKABLE void setKeymapPreset(const QString &preset);

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
    void hwdecModeChanged();
    void subtitleFontSizeChanged();
    void subtitlePositionChanged();
    void subtitleDelayChanged();
    void windowGeometryChanged();
    void alwaysOnTopChanged();
    void videoAdjustmentsChanged();
    void debandChanged();
    void hdrToneMappingChanged();
    void videoSharpenChanged();
    void themeChanged();
    void keymapPresetChanged();
    void equalizerChanged();
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
    QString m_hwdecMode = QStringLiteral("auto-copy");
    double m_subtitleFontSize = 38.0;
    double m_subtitlePosition = 100.0;
    double m_subtitleDelay = 0.0;
    int m_windowWidth = 1280;
    int m_windowHeight = 720;
    bool m_alwaysOnTop = false;

    // Video adjustments
    double m_brightness = 0.0;
    double m_contrast = 0.0;
    double m_saturation = 0.0;
    double m_hue = 0.0;
    double m_gamma = 0.0;

    // Video Processing & Quality
    bool m_debandEnabled = false;
    int m_debandIterations = 1;
    int m_debandThreshold = 32;
    int m_debandRange = 16;
    QString m_hdrToneMapping = QStringLiteral("auto");
    bool m_hdrComputePeak = true;
    double m_videoSharpen = 0.0;
    QString m_theme = QStringLiteral("midnight");
    QString m_keymapPreset = QStringLiteral("parvaz");

    // Equalizer
    bool m_equalizerEnabled = false;
    QString m_equalizerPreset = QStringLiteral("Flat");
    QVariantList m_equalizerGains = {QVariant(0.0), QVariant(0.0), QVariant(0.0), QVariant(0.0), QVariant(0.0),
                                     QVariant(0.0), QVariant(0.0), QVariant(0.0), QVariant(0.0), QVariant(0.0)};
};

} // namespace parvaz
