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

        emit volumeChanged();
        emit mutedChanged();
        emit audioNormalizerChanged();
        emit videoAspectChanged();
        emit performanceProfileChanged();
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
        { "performance_profile", m_performanceProfile.toStdString() }
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

} // namespace parvaz
