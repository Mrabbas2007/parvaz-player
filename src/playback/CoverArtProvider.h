#pragma once

#include <QObject>
#include <QString>

class QProcess;

namespace parvaz {

/// Finds and extracts audio cover art without blocking the GUI thread.
///
/// Lookup order: cover image next to the audio file → disk cache of
/// previously extracted art → ffmpeg extraction of the embedded picture
/// (asynchronous, one process at a time; a new request kills the in-flight
/// one). Results are delivered through the extracted() signal.
class CoverArtProvider : public QObject {
    Q_OBJECT
public:
    explicit CoverArtProvider(QObject *parent = nullptr);
    ~CoverArtProvider() override;

    /// Disk cache budget: 20MB of extracted cover art.
    static constexpr qint64 kMaxDiskCacheBytes = 20 * 1024 * 1024;

    /// Folder-cover scan + disk-cache lookup. Only stat() calls, safe on the
    /// GUI thread. Empty when nothing is available and extraction is needed.
    QString localCoverUrl(const QString &file) const;

    /// Starts an asynchronous ffmpeg extraction for `file`; emits extracted()
    /// when the pass (or its fallback) settles. Any in-flight extraction is
    /// cancelled first.
    void requestExtraction(const QString &file);

    /// Kills an in-flight extraction, if any.
    void cancel();

signals:
    /// url is a file:// URL on success, empty on failure.
    void extracted(const QString &file, const QString &url);

private:
    /// One ffmpeg pass; `fallbackFrame` selects between copying the embedded
    /// picture and transcoding the first video frame, chaining the other on
    /// failure.
    void startExtraction(const QString &file, const QString &outPath, bool fallbackFrame);

    QProcess *m_process = nullptr;
};

} // namespace parvaz
