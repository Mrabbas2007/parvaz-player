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
    // Cost is measured in KB (see putCache); 8MB of decoded thumbnails for lower RAM footprint.
    m_memCache.setMaxCost(8 * 1024);
    // 1 worker thread to avoid saturating low-end / no-GPU CPUs with parallel FFmpeg spawns.
    m_threadPool.setMaxThreadCount(1);

    // Portable disk cache dir: QStandardPaths CacheLocation is the XDG cache
    // (~/.cache) on Linux and %LOCALAPPDATA% on Windows — never a hard-coded
    // /tmp fallback, which does not exist as a portable runtime location.
    QString cacheBase = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cacheBase.isEmpty())
        cacheBase = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    m_diskCacheDir = cacheBase + QStringLiteral("/thumbnails");
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

ThumbnailResponse::ThumbnailResponse(const QString &id, const QSize &requestedSize,
                                     ThumbnailProvider *provider)
    : m_provider(provider)
{
    setAutoDelete(false);
    Q_UNUSED(requestedSize); // thumbnails have a fixed size set by the filter

    // Parse the id once here; run() only reads the parsed members.
    const int qIdx = id.lastIndexOf('?');
    const QString rawPath = (qIdx >= 0) ? id.left(qIdx) : id;
    const QString queryStr = (qIdx >= 0) ? id.mid(qIdx + 1) : QString();

    m_path = rawPath;
    if (m_path.contains(QLatin1Char('%'))) {
        m_path = QUrl::fromPercentEncoding(m_path.toUtf8());
    }
    if (m_path.startsWith(QLatin1String("file://"))) {
        m_path = QUrl(m_path).toLocalFile();
    }

    const QUrlQuery query(queryStr);
    if (query.hasQueryItem(QStringLiteral("t"))) {
        m_sec = qMax(0, qRound(query.queryItemValue(QStringLiteral("t")).toDouble()));
    }
    m_cacheKey = QStringLiteral("%1@%2").arg(m_path).arg(m_sec);

    QImage memImg;
    if (m_provider->getCached(m_cacheKey, memImg)) {
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

    const QString &path = m_path;
    const int sec = m_sec;
    const QString &cacheKey = m_cacheKey;

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

        // Resolve the binary via PATH lookup so Windows (ffmpeg.exe) is found too.
        const QString ffmpegBin = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
        proc.start(ffmpegBin.isEmpty() ? QStringLiteral("ffmpeg") : ffmpegBin, args);
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
