#include "playback/ThumbnailProvider.h"
#include "playback/CacheMaintenance.h"

#include <QStandardPaths>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QUrlQuery>
#include <QUrl>
#include <QDebug>

ThumbnailProvider *ThumbnailProvider::s_instance = nullptr;

ThumbnailProvider::ThumbnailProvider()
{
    s_instance = this;
    // Cost is measured in KB (see putCache); 16MB of decoded thumbnails.
    m_memCache.setMaxCost(16 * 1024);
    m_threadPool.setMaxThreadCount(2);

    const QString cacheBase = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    m_diskCacheDir = cacheBase.isEmpty()
                         ? QStringLiteral("/tmp/parvaz/thumbnails")
                         : (cacheBase + QStringLiteral("/thumbnails"));
    QDir().mkpath(m_diskCacheDir);
    // The disk cache is append-only otherwise; keep it bounded.
    parvaz::evictOldestCacheFiles(m_diskCacheDir, kMaxDiskCacheBytes);
}

ThumbnailProvider::~ThumbnailProvider()
{
    m_threadPool.waitForDone(500);
    s_instance = nullptr;
}

bool ThumbnailProvider::getCached(const QString &key, QImage &outImage)
{
    QMutexLocker locker(&m_cacheMutex);
    QImage *img = m_memCache.object(key);
    if (img) {
        outImage = *img;
        return true;
    }
    return false;
}

void ThumbnailProvider::putCache(const QString &key, const QImage &image)
{
    QMutexLocker locker(&m_cacheMutex);
    // Cost in KB, so the LRU bound is a memory bound rather than an item
    // count that ignores how big each decoded image is.
    m_memCache.insert(key, new QImage(image), qMax(1, int(image.sizeInBytes() / 1024)));
}

QQuickImageResponse *ThumbnailProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    return new ThumbnailResponse(id, requestedSize, this);
}

// ---- ThumbnailResponse -----------------------------------------------------

ThumbnailResponse::ThumbnailResponse(const QString &id, const QSize &requestedSize, ThumbnailProvider *provider)
    : m_id(id), m_requestedSize(requestedSize), m_provider(provider)
{
    setAutoDelete(false);

    const int qIdx = m_id.lastIndexOf('?');
    const QString rawPath = (qIdx >= 0) ? m_id.left(qIdx) : m_id;
    const QString queryStr = (qIdx >= 0) ? m_id.mid(qIdx + 1) : QString();

    QString path = rawPath;
    if (path.contains(QLatin1Char('%'))) {
        path = QUrl::fromPercentEncoding(path.toUtf8());
    }
    if (path.startsWith(QLatin1String("file://"))) {
        path = QUrl(path).toLocalFile();
    }

    double timestamp = 0.0;
    const QUrlQuery query(queryStr);
    if (query.hasQueryItem(QStringLiteral("t"))) {
        timestamp = query.queryItemValue(QStringLiteral("t")).toDouble();
    }

    const int sec = qMax(0, qRound(timestamp));
    const QString cacheKey = QStringLiteral("%1@%2").arg(path).arg(sec);

    QImage memImg;
    if (m_provider->getCached(cacheKey, memImg)) {
        m_texture = QQuickTextureFactory::textureFactoryForImage(memImg);
        QMetaObject::invokeMethod(this, &QQuickImageResponse::finished, Qt::QueuedConnection);
        return;
    }

    m_provider->threadPool().start(this);
}

ThumbnailResponse::~ThumbnailResponse()
{
    // The engine takes ownership of the returned texture factory.
}

QQuickTextureFactory *ThumbnailResponse::textureFactory() const
{
    return m_texture;
}

QString ThumbnailResponse::errorString() const
{
    if (!m_error.isEmpty())
        return m_error;
    if (!m_texture)
        return QStringLiteral("No texture produced");
    return QString();
}

void ThumbnailResponse::cancel()
{
    m_cancelled.store(true);
    QMutexLocker locker(&m_procMutex);
    if (m_process) {
        m_process->kill();
    }
}

void ThumbnailResponse::run()
{
    if (m_cancelled.load()) {
        m_error = QStringLiteral("Cancelled");
        emit finished();
        return;
    }

    const int qIdx = m_id.lastIndexOf('?');
    const QString rawPath = (qIdx >= 0) ? m_id.left(qIdx) : m_id;
    const QString queryStr = (qIdx >= 0) ? m_id.mid(qIdx + 1) : QString();

    QString path = rawPath;
    if (path.contains(QLatin1Char('%'))) {
        path = QUrl::fromPercentEncoding(path.toUtf8());
    }
    if (path.startsWith(QLatin1String("file://"))) {
        path = QUrl(path).toLocalFile();
    }

    double timestamp = 0.0;
    const QUrlQuery query(queryStr);
    if (query.hasQueryItem(QStringLiteral("t"))) {
        timestamp = query.queryItemValue(QStringLiteral("t")).toDouble();
    }

    const int sec = qMax(0, qRound(timestamp));
    const QString cacheKey = QStringLiteral("%1@%2").arg(path).arg(sec);

    const QByteArray hash = QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha1).toHex();
    const QString subDir = m_provider->diskCacheDir() + QStringLiteral("/") + QString::fromLatin1(hash);
    const QString diskFile = subDir + QStringLiteral("/%1.jpg").arg(sec);

    QImage resultImg;
    if (QFile::exists(diskFile)) {
        resultImg.load(diskFile);
    } else {
        if (m_cancelled.load()) {
            emit finished();
            return;
        }

        QDir().mkpath(subDir);

        QProcess proc;
        {
            QMutexLocker locker(&m_procMutex);
            if (m_cancelled.load()) {
                m_error = QStringLiteral("Cancelled");
                emit finished();
                return;
            }
            m_process = &proc;
        }

        QStringList args;
        args << QStringLiteral("-skip_frame") << QStringLiteral("nokey")
             << QStringLiteral("-ss") << QString::number(sec)
             << QStringLiteral("-i") << path
             << QStringLiteral("-map") << QStringLiteral("0:v:0")
             << QStringLiteral("-an") << QStringLiteral("-sn") << QStringLiteral("-dn")
             << QStringLiteral("-frames:v") << QStringLiteral("1")
             << QStringLiteral("-update") << QStringLiteral("1")
             << QStringLiteral("-vf") << QStringLiteral("scale=160:90:force_original_aspect_ratio=decrease,pad=160:90:(ow-iw)/2:(oh-ih)/2:black")
             << QStringLiteral("-q:v") << QStringLiteral("3")
             << QStringLiteral("-f") << QStringLiteral("image2")
             << QStringLiteral("-y") << diskFile;

        proc.start(QStringLiteral("ffmpeg"), args);
        proc.waitForFinished(1200);

        {
            QMutexLocker locker(&m_procMutex);
            m_process = nullptr;
        }

        if (!m_cancelled.load() && QFile::exists(diskFile)) {
            resultImg.load(diskFile);
        }
    }

    if (!resultImg.isNull() && !m_cancelled.load()) {
        m_provider->putCache(cacheKey, resultImg);
        m_texture = QQuickTextureFactory::textureFactoryForImage(resultImg);
    }

    if (!m_texture && m_error.isEmpty()) {
        m_error = QStringLiteral("Failed to produce thumbnail texture");
    }

    emit finished();
}
