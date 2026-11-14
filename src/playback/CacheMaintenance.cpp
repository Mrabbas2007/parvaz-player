#include "playback/CacheMaintenance.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

#include <algorithm>

namespace parvaz {

void evictOldestCacheFiles(const QString &dir, qint64 maxBytes)
{
    if (maxBytes <= 0 || dir.isEmpty())
        return;

    struct Entry {
        QString path;
        qint64 size;
        qint64 mtime;
    };

    QList<Entry> entries;
    qint64 total = 0;

    QDirIterator it(dir, QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QFileInfo info(it.next());
        entries.append({info.absoluteFilePath(), info.size(),
                        info.lastModified().toSecsSinceEpoch()});
        total += entries.last().size;
    }

    if (total <= maxBytes)
        return;

    std::sort(entries.begin(), entries.end(),
              [](const Entry &a, const Entry &b) { return a.mtime < b.mtime; });

    for (const Entry &e : entries) {
        if (total <= maxBytes)
            break;
        QFile::remove(e.path);
        total -= e.size;
    }
}

} // namespace parvaz
