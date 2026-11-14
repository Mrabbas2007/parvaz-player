#pragma once

#include <QString>

namespace parvaz {

/// Deletes oldest-modified files under `dir` (recursively) until the total
/// size fits maxBytes. Both media caches (thumbnails, cover art) grow
/// forever without this. Called once at startup by each cache owner; only
/// stat() and unlink() calls, cheap on the GUI thread.
void evictOldestCacheFiles(const QString &dir, qint64 maxBytes);

} // namespace parvaz
