#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
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
    Q_PROPERTY(QString mediaTitle READ mediaTitle NOTIFY mediaTitleChanged)
    Q_PROPERTY(QString hwdec READ hwdec NOTIFY hwdecChanged)

public:
    explicit MpvObject(QObject *parent = nullptr);
    ~MpvObject() override;

    static MpvObject *instance() { return s_instance; }
    mpv_handle *handle() const { return m_mpv; }

    double position() const { return m_position; }
    double duration() const { return m_duration; }
    bool paused() const { return m_paused; }
    QString mediaTitle() const { return m_mediaTitle; }
    QString hwdec() const { return m_hwdec; }

    void setPaused(bool paused);

    Q_INVOKABLE void loadFile(const QString &path);

    /// Called (marshalled) from the render thread once the mpv render context
    /// exists. Playback must not start before that, otherwise mpv fails to
    /// initialise the video output ("vo=libmpv" with no consumer).
    void setRenderReady();
    Q_INVOKABLE void togglePause();
    Q_INVOKABLE void seekRelative(double seconds);
    Q_INVOKABLE void setSpeed(double speed);
    Q_INVOKABLE QString formatTime(double seconds) const;

    /// "hwdec=vaapi drop=0 decoder-drop=0" for benchmarks.
    QString statsLine() const;

signals:
    void positionChanged();
    void durationChanged();
    void pausedChanged();
    void mediaTitleChanged();
    void hwdecChanged();
    void fileLoaded();

private slots:
    void onWakeup();

private:
    void drainEvents();
    static void wakeupCallback(void *ctx);

    static MpvObject *s_instance;
    mpv_handle *m_mpv = nullptr;
    std::atomic_bool m_wakeupPending{false};

    bool m_renderReady = false;
    QString m_pendingFile;

    double m_position = 0.0;
    double m_duration = 0.0;
    bool m_paused = true;
    QString m_mediaTitle;
    QString m_hwdec;
};
