#include "config/ConfigManager.h"
#include "mpv/MpvObject.h"

#define TOML_EXCEPTIONS 0
#include "tomlplusplus/toml.hpp"

#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace parvaz {

ConfigManager *ConfigManager::s_instance = nullptr;

ConfigManager::ConfigManager(MpvObject *mpv, const QString &explicitPath, QObject *parent)
    : QObject(parent)
    , m_mpv(mpv)
{
    s_instance = this;

    m_saveDebounceTimer.setSingleShot(true);
    m_saveDebounceTimer.setInterval(500); // 500ms debounce
    connect(&m_saveDebounceTimer, &QTimer::timeout, this, &ConfigManager::writeToDisk);

    if (!explicitPath.isEmpty()) {
        m_configPath = explicitPath;
    } else {
        const QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        m_configPath = configDir + QStringLiteral("/config.toml");
    }

    // Ensure directory exists
    const QFileInfo fi(m_configPath);
    QDir().mkpath(fi.absolutePath());

    load();

    // Setup file watcher for hot-reloading
    if (QFile::exists(m_configPath)) {
        m_watcher.addPath(m_configPath);
    }
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &ConfigManager::onFileChanged);

    // Synchronize changes from MpvObject back to Config
    if (m_mpv) {
        connect(m_mpv, &MpvObject::volumeChanged, this, [this]() {
            if (!m_applyingToPlayer && m_mpv)
                setVolume(m_mpv->volume());
        });
        connect(m_mpv, &MpvObject::mutedChanged, this, [this]() {
            if (!m_applyingToPlayer && m_mpv)
                setMuted(m_mpv->muted());
        });
        connect(m_mpv, &MpvObject::audioNormalizerChanged, this, [this]() {
            if (!m_applyingToPlayer && m_mpv)
                setAudioNormalizer(m_mpv->audioNormalizer());
        });
        connect(m_mpv, &MpvObject::videoAspectChanged, this, [this]() {
            if (!m_applyingToPlayer && m_mpv)
                setVideoAspect(m_mpv->videoAspect());
        });
        connect(m_mpv, &MpvObject::performanceProfileChanged, this, [this]() {
            if (!m_applyingToPlayer && m_mpv)
                setPerformanceProfile(m_mpv->performanceProfile());
        });
        connect(m_mpv, &MpvObject::hwdecModeChanged, this, [this]() {
            if (!m_applyingToPlayer && m_mpv)
                setHwdecMode(m_mpv->hwdecMode());
        });
        connect(m_mpv, &MpvObject::subtitleFontSizeChanged, this, [this]() {
            if (!m_applyingToPlayer && m_mpv)
                setSubtitleFontSize(m_mpv->subtitleFontSize());
        });
        connect(m_mpv, &MpvObject::subtitlePositionChanged, this, [this]() {
            if (!m_applyingToPlayer && m_mpv)
                setSubtitlePosition(m_mpv->subtitlePosition());
        });
        connect(m_mpv, &MpvObject::subtitleDelayChanged, this, [this]() {
            if (!m_applyingToPlayer && m_mpv)
                setSubtitleDelay(m_mpv->subtitleDelay());
        });
        connect(m_mpv, &MpvObject::brightnessChanged, this, [this]() {
            if (!m_applyingToPlayer && m_mpv)
                setBrightness(m_mpv->brightness());
        });
        connect(m_mpv, &MpvObject::contrastChanged, this, [this]() {
            if (!m_applyingToPlayer && m_mpv)
                setContrast(m_mpv->contrast());
        });
        connect(m_mpv, &MpvObject::saturationChanged, this, [this]() {
            if (!m_applyingToPlayer && m_mpv)
                setSaturation(m_mpv->saturation());
        });
        connect(m_mpv, &MpvObject::hueChanged, this, [this]() {
            if (!m_applyingToPlayer && m_mpv)
                setHue(m_mpv->hue());
        });
        connect(m_mpv, &MpvObject::gammaChanged, this, [this]() {
            if (!m_applyingToPlayer && m_mpv)
                setGamma(m_mpv->gamma());
        });
        connect(m_mpv, &MpvObject::equalizerEnabledChanged, this, [this]() {
            if (!m_applyingToPlayer && m_mpv)
                setEqualizerEnabled(m_mpv->equalizerEnabled());
        });
        connect(m_mpv, &MpvObject::equalizerPresetChanged, this, [this]() {
            if (!m_applyingToPlayer && m_mpv)
                setEqualizerPreset(m_mpv->equalizerPreset());
        });
        connect(m_mpv, &MpvObject::equalizerGainsChanged, this, [this]() {
            if (!m_applyingToPlayer && m_mpv)
                setEqualizerGains(m_mpv->equalizerGains());
        });
        connect(m_mpv, &MpvObject::debandChanged, this, [this]() {
            if (!m_applyingToPlayer && m_mpv) {
                m_debandEnabled = m_mpv->debandEnabled();
                m_debandIterations = m_mpv->debandIterations();
                m_debandThreshold = m_mpv->debandThreshold();
                m_debandRange = m_mpv->debandRange();
                emit debandChanged();
                scheduleSave();
            }
        });
        connect(m_mpv, &MpvObject::hdrToneMappingChanged, this, [this]() {
            if (!m_applyingToPlayer && m_mpv) {
                m_hdrToneMapping = m_mpv->hdrToneMapping();
                m_hdrComputePeak = m_mpv->hdrComputePeak();
                emit hdrToneMappingChanged();
                scheduleSave();
            }
        });
        connect(m_mpv, &MpvObject::videoSharpenChanged, this, [this]() {
            if (!m_applyingToPlayer && m_mpv) {
                m_videoSharpen = m_mpv->videoSharpen();
                emit videoSharpenChanged();
                scheduleSave();
            }
        });

        applyToPlayer();
    }
}

ConfigManager::~ConfigManager()
{
    if (m_saveDebounceTimer.isActive()) {
        m_saveDebounceTimer.stop();
        writeToDisk();
    }
    s_instance = nullptr;
}

void ConfigManager::load()
{
    if (!QFile::exists(m_configPath)) {
        // Create initial default file
        writeToDisk();
        return;
    }

    m_loading = true;
    std::string pathStd = m_configPath.toStdString();
    toml::parse_result result = toml::parse_file(pathStd);

    if (result) {
        const toml::table &tbl = result.table();

        // [audio]
        if (auto audio = tbl["audio"].as_table()) {
            if (auto v = (*audio)["volume"].value<int64_t>())
                m_volume = static_cast<int>(*v);
            if (auto m = (*audio)["muted"].value<bool>())
                m_muted = *m;
            if (auto n = (*audio)["normalizer"].value<bool>())
                m_audioNormalizer = *n;
        }

        // [video]
        if (auto video = tbl["video"].as_table()) {
            if (auto a = (*video)["aspect"].value<std::string>())
                m_videoAspect = QString::fromStdString(*a);
            if (auto p = (*video)["performance_profile"].value<std::string>())
                m_performanceProfile = QString::fromStdString(*p);
            if (auto hm = (*video)["hwdec_mode"].value<std::string>())
                m_hwdecMode = QString::fromStdString(*hm);
            if (auto b = (*video)["brightness"].value<double>())
                m_brightness = *b;
            if (auto c = (*video)["contrast"].value<double>())
                m_contrast = *c;
            if (auto s = (*video)["saturation"].value<double>())
                m_saturation = *s;
            if (auto h = (*video)["hue"].value<double>())
                m_hue = *h;
            if (auto g = (*video)["gamma"].value<double>())
                m_gamma = *g;
            if (auto db = (*video)["deband_enabled"].value<bool>())
                m_debandEnabled = *db;
            if (auto dbi = (*video)["deband_iterations"].value<int64_t>())
                m_debandIterations = static_cast<int>(*dbi);
            if (auto dbt = (*video)["deband_threshold"].value<int64_t>())
                m_debandThreshold = static_cast<int>(*dbt);
            if (auto dbr = (*video)["deband_range"].value<int64_t>())
                m_debandRange = static_cast<int>(*dbr);
            if (auto htm = (*video)["hdr_tone_mapping"].value<std::string>())
                m_hdrToneMapping = QString::fromStdString(*htm);
            if (auto hcp = (*video)["hdr_compute_peak"].value<bool>())
                m_hdrComputePeak = *hcp;
            if (auto sh = (*video)["sharpen"].value<double>())
                m_videoSharpen = *sh;
        }

        // [equalizer]
        if (auto eq = tbl["equalizer"].as_table()) {
            if (auto e = (*eq)["enabled"].value<bool>())
                m_equalizerEnabled = *e;
            if (auto p = (*eq)["preset"].value<std::string>())
                m_equalizerPreset = QString::fromStdString(*p);
            if (auto arr = (*eq)["gains"].as_array()) {
                m_equalizerGains.clear();
                for (const auto &elem : *arr) {
                    if (auto g = elem.value<double>())
                        m_equalizerGains.append(*g);
                }
            }
        }

        // [subtitles]
        if (auto subs = tbl["subtitles"].as_table()) {
            if (auto s = (*subs)["font_size"].value<double>())
                m_subtitleFontSize = *s;
            if (auto p = (*subs)["position"].value<double>())
                m_subtitlePosition = *p;
            if (auto d = (*subs)["delay"].value<double>())
                m_subtitleDelay = *d;
        }

        // [window]
        if (auto win = tbl["window"].as_table()) {
            if (auto w = (*win)["width"].value<int64_t>())
                m_windowWidth = static_cast<int>(*w);
            if (auto h = (*win)["height"].value<int64_t>())
                m_windowHeight = static_cast<int>(*h);
            if (auto top = (*win)["always_on_top"].value<bool>())
                m_alwaysOnTop = *top;
        }

        // [appearance]
        if (auto app = tbl["appearance"].as_table()) {
            if (auto th = (*app)["theme"].value<std::string>())
                m_theme = QString::fromStdString(*th);
        }

        // [shortcuts]
        if (auto sc = tbl["shortcuts"].as_table()) {
            if (auto kp = (*sc)["keymap_preset"].value<std::string>())
                m_keymapPreset = QString::fromStdString(*kp);
        }

        emit volumeChanged();
        emit mutedChanged();
        emit audioNormalizerChanged();
        emit videoAspectChanged();
        emit performanceProfileChanged();
        emit debandChanged();
        emit hdrToneMappingChanged();
        emit videoSharpenChanged();
        emit themeChanged();
        emit keymapPresetChanged();
        emit subtitleFontSizeChanged();
        emit subtitlePositionChanged();
        emit subtitleDelayChanged();
        emit windowGeometryChanged();
        emit alwaysOnTopChanged();
        emit configReloaded();
    } else {
        std::fprintf(stderr, "parvaz: failed to parse config TOML: %s\n",
                     result.error().description().data());
    }

    m_loading = false;
}

void ConfigManager::applyToPlayer()
{
    if (!m_mpv)
        return;

    m_applyingToPlayer = true;
    m_mpv->setVolume(m_volume);
    m_mpv->setMuted(m_muted);
    m_mpv->setAudioNormalizer(m_audioNormalizer);
    m_mpv->setVideoAspect(m_videoAspect);
    m_mpv->setPerformanceProfile(m_performanceProfile);
    m_mpv->setHwdecMode(m_hwdecMode);
    m_mpv->setBrightness(m_brightness);
    m_mpv->setContrast(m_contrast);
    m_mpv->setSaturation(m_saturation);
    m_mpv->setHue(m_hue);
    m_mpv->setGamma(m_gamma);
    m_mpv->setDebandEnabled(m_debandEnabled);
    m_mpv->setDebandIterations(m_debandIterations);
    m_mpv->setDebandThreshold(m_debandThreshold);
    m_mpv->setDebandRange(m_debandRange);
    m_mpv->setHdrToneMapping(m_hdrToneMapping);
    m_mpv->setHdrComputePeak(m_hdrComputePeak);
    m_mpv->setVideoSharpen(m_videoSharpen);
    m_mpv->setEqualizerEnabled(m_equalizerEnabled);
    if (!m_equalizerPreset.isEmpty())
        m_mpv->applyEqualizerPreset(m_equalizerPreset);
    if (!m_equalizerGains.isEmpty()) {
        for (int i = 0; i < m_equalizerGains.size(); ++i) {
            m_mpv->setEqualizerBandGain(i, m_equalizerGains[i].toDouble());
        }
    }
    m_mpv->setSubtitleFontSize(m_subtitleFontSize);
    m_mpv->setSubtitlePosition(m_subtitlePosition);
    m_mpv->setSubtitleDelay(m_subtitleDelay);
    m_applyingToPlayer = false;
}

void ConfigManager::scheduleSave()
{
    if (m_loading)
        return;
    m_saveDebounceTimer.start();
}

void ConfigManager::saveNow()
{
    m_saveDebounceTimer.stop();
    writeToDisk();
}

void ConfigManager::writeToDisk()
{
    toml::table tbl;

    // [audio]
    tbl.insert_or_assign("audio", toml::table{
        { "volume", m_volume },
        { "muted", m_muted },
        { "normalizer", m_audioNormalizer }
    });

    // [video]
    tbl.insert_or_assign("video", toml::table{
        { "aspect", m_videoAspect.toStdString() },
        { "performance_profile", m_performanceProfile.toStdString() },
        { "hwdec_mode", m_hwdecMode.toStdString() },
        { "brightness", m_brightness },
        { "contrast", m_contrast },
        { "saturation", m_saturation },
        { "hue", m_hue },
        { "gamma", m_gamma },
        { "deband_enabled", m_debandEnabled },
        { "deband_iterations", m_debandIterations },
        { "deband_threshold", m_debandThreshold },
        { "deband_range", m_debandRange },
        { "hdr_tone_mapping", m_hdrToneMapping.toStdString() },
        { "hdr_compute_peak", m_hdrComputePeak },
        { "sharpen", m_videoSharpen }
    });

    // [equalizer]
    toml::array eqGainsArray;
    for (const auto &v : m_equalizerGains) {
        eqGainsArray.push_back(v.toDouble());
    }
    tbl.insert_or_assign("equalizer", toml::table{
        { "enabled", m_equalizerEnabled },
        { "preset", m_equalizerPreset.toStdString() },
        { "gains", eqGainsArray }
    });

    // [subtitles]
    tbl.insert_or_assign("subtitles", toml::table{
        { "font_size", m_subtitleFontSize },
        { "position", m_subtitlePosition },
        { "delay", m_subtitleDelay }
    });

    // [window]
    tbl.insert_or_assign("window", toml::table{
        { "width", m_windowWidth },
        { "height", m_windowHeight },
        { "always_on_top", m_alwaysOnTop }
    });

    // [appearance]
    tbl.insert_or_assign("appearance", toml::table{
        { "theme", m_theme.toStdString() }
    });

    // [shortcuts]
    tbl.insert_or_assign("shortcuts", toml::table{
        { "keymap_preset", m_keymapPreset.toStdString() }
    });

    std::stringstream ss;
    ss << "# Parvaz Player Configuration\n";
    ss << "# Hot-reloaded automatically when modified.\n\n";
    ss << tbl;

    const std::string content = ss.str();

    // Temporarily disable watcher to avoid reacting to our own write
    if (m_watcher.files().contains(m_configPath)) {
        m_watcher.removePath(m_configPath);
    }

    QFile file(m_configPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        file.write(content.c_str(), content.size());
        file.close();
    }

    if (QFile::exists(m_configPath)) {
        m_watcher.addPath(m_configPath);
    }
}

void ConfigManager::onFileChanged(const QString &path)
{
    if (path == m_configPath) {
        // Some editors delete and recreate the file, which drops the watch
        if (!m_watcher.files().contains(m_configPath) && QFile::exists(m_configPath)) {
            m_watcher.addPath(m_configPath);
        }
        std::fprintf(stderr, "parvaz: config file modified on disk, reloading...\n");
        load();
        applyToPlayer();
        if (m_mpv)
            m_mpv->showOsd(QStringLiteral("Config reloaded from disk"));
    }
}

void ConfigManager::reload()
{
    load();
    applyToPlayer();
}

void ConfigManager::setVolume(int v)
{
    v = qBound(0, v, 150);
    if (m_volume == v)
        return;
    m_volume = v;
    emit volumeChanged();
    scheduleSave();
}

void ConfigManager::setMuted(bool m)
{
    if (m_muted == m)
        return;
    m_muted = m;
    emit mutedChanged();
    scheduleSave();
}

void ConfigManager::setAudioNormalizer(bool n)
{
    if (m_audioNormalizer == n)
        return;
    m_audioNormalizer = n;
    emit audioNormalizerChanged();
    scheduleSave();
}

void ConfigManager::setVideoAspect(const QString &a)
{
    if (m_videoAspect == a || a.isEmpty())
        return;
    m_videoAspect = a;
    emit videoAspectChanged();
    scheduleSave();
}

void ConfigManager::setPerformanceProfile(const QString &p)
{
    if (m_performanceProfile == p || p.isEmpty())
        return;
    m_performanceProfile = p;
    emit performanceProfileChanged();
    scheduleSave();
}

void ConfigManager::setHwdecMode(const QString &m)
{
    const QString trimmed = m.trimmed();
    if (m_hwdecMode == trimmed || trimmed.isEmpty())
        return;
    m_hwdecMode = trimmed;
    emit hwdecModeChanged();

    if (m_mpv && !m_applyingToPlayer) {
        m_mpv->setHwdecMode(m_hwdecMode);
    }
    scheduleSave();
}

void ConfigManager::setSubtitleFontSize(double s)
{
    if (qFuzzyCompare(m_subtitleFontSize, s))
        return;
    m_subtitleFontSize = s;
    emit subtitleFontSizeChanged();
    scheduleSave();
}

void ConfigManager::setSubtitlePosition(double p)
{
    if (qFuzzyCompare(m_subtitlePosition, p))
        return;
    m_subtitlePosition = p;
    emit subtitlePositionChanged();
    scheduleSave();
}

void ConfigManager::setSubtitleDelay(double d)
{
    if (qFuzzyCompare(m_subtitleDelay, d))
        return;
    m_subtitleDelay = d;
    emit subtitleDelayChanged();
    scheduleSave();
}

void ConfigManager::setWindowSize(int w, int h)
{
    if (w <= 0 || h <= 0)
        return;
    if (m_windowWidth == w && m_windowHeight == h)
        return;
    m_windowWidth = w;
    m_windowHeight = h;
    emit windowGeometryChanged();
    scheduleSave();
}

void ConfigManager::setWindowWidth(int w)
{
    setWindowSize(w, m_windowHeight);
}

void ConfigManager::setWindowHeight(int h)
{
    setWindowSize(m_windowWidth, h);
}

void ConfigManager::setAlwaysOnTop(bool onTop)
{
    if (m_alwaysOnTop == onTop)
        return;
    m_alwaysOnTop = onTop;
    emit alwaysOnTopChanged();
    scheduleSave();
}

void ConfigManager::setBrightness(double b)
{
    if (qFuzzyCompare(m_brightness, b))
        return;
    m_brightness = b;
    emit videoAdjustmentsChanged();
    scheduleSave();
}

void ConfigManager::setContrast(double c)
{
    if (qFuzzyCompare(m_contrast, c))
        return;
    m_contrast = c;
    emit videoAdjustmentsChanged();
    scheduleSave();
}

void ConfigManager::setSaturation(double s)
{
    if (qFuzzyCompare(m_saturation, s))
        return;
    m_saturation = s;
    emit videoAdjustmentsChanged();
    scheduleSave();
}

void ConfigManager::setHue(double h)
{
    if (qFuzzyCompare(m_hue, h))
        return;
    m_hue = h;
    emit videoAdjustmentsChanged();
    scheduleSave();
}

void ConfigManager::setGamma(double g)
{
    if (qFuzzyCompare(m_gamma, g))
        return;
    m_gamma = g;
    emit videoAdjustmentsChanged();
    scheduleSave();
}

void ConfigManager::setEqualizerEnabled(bool e)
{
    if (m_equalizerEnabled == e)
        return;
    m_equalizerEnabled = e;
    emit equalizerChanged();
    scheduleSave();
}

void ConfigManager::setEqualizerPreset(const QString &p)
{
    if (m_equalizerPreset == p || p.isEmpty())
        return;
    m_equalizerPreset = p;
    emit equalizerChanged();
    scheduleSave();
}

void ConfigManager::setEqualizerGains(const QVariantList &gains)
{
    if (m_equalizerGains == gains)
        return;
    m_equalizerGains = gains;
    emit equalizerChanged();
    scheduleSave();
}

void ConfigManager::setDebandEnabled(bool enabled)
{
    if (m_debandEnabled == enabled)
        return;
    m_debandEnabled = enabled;
    emit debandChanged();
    if (!m_applyingToPlayer && m_mpv)
        m_mpv->setDebandEnabled(enabled);
    scheduleSave();
}

void ConfigManager::setDebandIterations(int iter)
{
    iter = qBound(1, iter, 4);
    if (m_debandIterations == iter)
        return;
    m_debandIterations = iter;
    emit debandChanged();
    if (!m_applyingToPlayer && m_mpv)
        m_mpv->setDebandIterations(iter);
    scheduleSave();
}

void ConfigManager::setDebandThreshold(int thresh)
{
    thresh = qBound(16, thresh, 128);
    if (m_debandThreshold == thresh)
        return;
    m_debandThreshold = thresh;
    emit debandChanged();
    if (!m_applyingToPlayer && m_mpv)
        m_mpv->setDebandThreshold(thresh);
    scheduleSave();
}

void ConfigManager::setDebandRange(int range)
{
    range = qBound(4, range, 32);
    if (m_debandRange == range)
        return;
    m_debandRange = range;
    emit debandChanged();
    if (!m_applyingToPlayer && m_mpv)
        m_mpv->setDebandRange(range);
    scheduleSave();
}

void ConfigManager::setHdrToneMapping(const QString &algorithm)
{
    const QString algo = algorithm.trimmed().toLower();
    if (m_hdrToneMapping == algo || algo.isEmpty())
        return;
    m_hdrToneMapping = algo;
    emit hdrToneMappingChanged();
    if (!m_applyingToPlayer && m_mpv)
        m_mpv->setHdrToneMapping(algo);
    scheduleSave();
}

void ConfigManager::setHdrComputePeak(bool computePeak)
{
    if (m_hdrComputePeak == computePeak)
        return;
    m_hdrComputePeak = computePeak;
    emit hdrToneMappingChanged();
    if (!m_applyingToPlayer && m_mpv)
        m_mpv->setHdrComputePeak(computePeak);
    scheduleSave();
}

void ConfigManager::setVideoSharpen(double amount)
{
    amount = qBound(0.0, amount, 2.0);
    if (qFuzzyCompare(m_videoSharpen, amount))
        return;
    m_videoSharpen = amount;
    emit videoSharpenChanged();
    if (!m_applyingToPlayer && m_mpv)
        m_mpv->setVideoSharpen(amount);
    scheduleSave();
}

void ConfigManager::setTheme(const QString &theme)
{
    if (m_theme == theme || theme.isEmpty())
        return;
    m_theme = theme;
    emit themeChanged();
    scheduleSave();
}

void ConfigManager::setKeymapPreset(const QString &preset)
{
    if (m_keymapPreset == preset || preset.isEmpty())
        return;
    m_keymapPreset = preset;
    emit keymapPresetChanged();
    scheduleSave();
}

} // namespace parvaz
