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
    QString m_id;
    QSize m_requestedSize;
    ThumbnailProvider *m_provider;
    QQuickTextureFactory *m_texture = nullptr;
    QString m_error;
    std::atomic<bool> m_cancelled{false};
    QMutex m_procMutex;
    QProcess *m_process = nullptr;
};
