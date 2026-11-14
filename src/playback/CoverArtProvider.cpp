#include "playback/CoverArtProvider.h"
#include "playback/CacheMaintenance.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

namespace parvaz {

CoverArtProvider::CoverArtProvider(QObject *parent)
    : QObject(parent)
{
    // Extracted covers accumulate one .jpg per audio file ever played; keep
    // the directory bounded.
    evictOldestCacheFiles(
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
            + QStringLiteral("/covers"),
        kMaxDiskCacheBytes);
}

CoverArtProvider::~CoverArtProvider()
{
    cancel();
}

/// Cover art cache: one .jpg per audio file, keyed by the file path hash.
static QString cachePathFor(const QString &file)
{
    const QString cacheDir =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/covers");
    const QByteArray hash = QCryptographicHash::hash(file.toUtf8(), QCryptographicHash::Sha1).toHex();
    return cacheDir + QStringLiteral("/") + QString::fromLatin1(hash) + QStringLiteral(".jpg");
}

QString CoverArtProvider::localCoverUrl(const QString &file) const
{
    // 1. A cover image sitting next to the audio file wins.
    const QFileInfo fi(file);
    const QDir dir = fi.absoluteDir();
    const QString baseName = fi.completeBaseName();

    const QStringList candidates = {
        QStringLiteral("cover.jpg"), QStringLiteral("cover.png"), QStringLiteral("cover.jpeg"), QStringLiteral("cover.webp"),
        QStringLiteral("folder.jpg"), QStringLiteral("folder.png"), QStringLiteral("folder.jpeg"),
        QStringLiteral("front.jpg"), QStringLiteral("front.png"), QStringLiteral("front.jpeg"),
        QStringLiteral("album.jpg"), QStringLiteral("album.png"), QStringLiteral("album.jpeg"),
        baseName + QStringLiteral(".jpg"), baseName + QStringLiteral(".png"), baseName + QStringLiteral(".jpeg")
    };

    for (const QString &candidate : candidates) {
        if (dir.exists(candidate))
            return QUrl::fromLocalFile(dir.absoluteFilePath(candidate)).toString();
    }

    // 2. Previously extracted embedded art.
    const QString cached = cachePathFor(file);
    if (QFileInfo(cached).isReadable() && QFileInfo(cached).size() > 0)
        return QUrl::fromLocalFile(cached).toString();

    return QString();
}

void CoverArtProvider::requestExtraction(const QString &file)
{
    cancel();
    startExtraction(file, cachePathFor(file), false);
}

void CoverArtProvider::cancel()
{
    if (m_process) {
        m_process->kill();
        m_process->deleteLater();
        m_process = nullptr;
    }
}

void CoverArtProvider::startExtraction(const QString &file, const QString &outPath, bool fallbackFrame)
{
    QDir().mkpath(QFileInfo(outPath).absolutePath());

    auto *proc = new QProcess(this);
    m_process = proc;

    QStringList args;
    if (fallbackFrame) {
        // Copying the attached picture failed (e.g. PNG cover into a .jpg
        // container): transcode the first frame of the video stream instead.
        args << QStringLiteral("-y")
             << QStringLiteral("-i") << file
             << QStringLiteral("-map") << QStringLiteral("0:v:0")
             << QStringLiteral("-frames:v") << QStringLiteral("1")
             << QStringLiteral("-q:v") << QStringLiteral("2")
             << outPath;
    } else {
        args << QStringLiteral("-y")
             << QStringLiteral("-i") << file
             << QStringLiteral("-an")
             << QStringLiteral("-vcodec") << QStringLiteral("copy")
             << outPath;
    }

    // Request identity is captured in the lambda: a finished() from a killed
    // or superseded process must never touch the newest request's state.
    connect(proc, &QProcess::finished, this,
            [this, proc, file, outPath, fallbackFrame](int, QProcess::ExitStatus status) {
        proc->deleteLater();
        const bool superseded = (m_process != proc);
        if (!superseded)
            m_process = nullptr;

        // A killed ffmpeg leaves a partial file behind; keeping it would let
        // the disk-cache check serve a corrupt image later.
        if (status == QProcess::CrashExit)
            QFile::remove(outPath);

        if (superseded)
            return;

        if (QFileInfo(outPath).size() > 0) {
            emit extracted(file, QUrl::fromLocalFile(outPath).toString());
            return;
        }
        QFile::remove(outPath);

        if (!fallbackFrame) {
            startExtraction(file, outPath, true);
        } else {
            emit extracted(file, QString());
        }
    });

    proc->start(QStringLiteral("ffmpeg"), args);
}

} // namespace parvaz
