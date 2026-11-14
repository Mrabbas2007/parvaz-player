#pragma once

#include <QQuickAsyncImageProvider>
#include <QQuickTextureFactory>
#include <QImage>
#include <QCache>
#include <QMutex>
#include <QThreadPool>
#include <QRunnable>
#include <atomic>

class ThumbnailResponse;
class QProcess;

class ThumbnailProvider : public QQuickAsyncImageProvider
{
public:
    ThumbnailProvider();
    ~ThumbnailProvider() override;

    /// Disk cache budget: 50MB of encoded thumbnails.
    static constexpr qint64 kMaxDiskCacheBytes = 50 * 1024 * 1024;

    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;

    static ThumbnailProvider *instance() { return s_instance; }

    bool getCached(const QString &key, QImage &outImage);
    void putCache(const QString &key, const QImage &image);

    QThreadPool &threadPool() { return m_threadPool; }
    QString diskCacheDir() const { return m_diskCacheDir; }

private:
    static ThumbnailProvider *s_instance;
    QCache<QString, QImage> m_memCache;
    mutable QMutex m_cacheMutex;
    QThreadPool m_threadPool;
    QString m_diskCacheDir;
};

class ThumbnailResponse : public QQuickImageResponse, public QRunnable
{
    Q_OBJECT
public:
    ThumbnailResponse(const QString &id, const QSize &requestedSize, ThumbnailProvider *provider);
    ~ThumbnailResponse() override;

    QQuickTextureFactory *textureFactory() const override;
    QString errorString() const override;
    void cancel() override;
    void run() override;

private:
    /// Parsed once in the constructor (GUI thread), read-only afterwards on
    /// the pool thread.
    QString m_path;
    int m_sec = 0;
    QString m_cacheKey;
    ThumbnailProvider *m_provider;
    QQuickTextureFactory *m_texture = nullptr;
    QString m_error;
    std::atomic<bool> m_cancelled{false};
    QMutex m_procMutex;
    QProcess *m_process = nullptr;
};
