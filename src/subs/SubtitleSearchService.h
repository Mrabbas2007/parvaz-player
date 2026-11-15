#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class QIODevice;

/// Asynchronous online subtitle search, matching and download service.
///
/// Implements:
/// 1. Official OpenSubtitles 64-bit file checksum algorithm (fast 128KB I/O).
/// 2. Heuristic video filename cleaner for title/year/season extraction.
/// 3. Non-blocking REST queries for OpenSubtitles & Subdl catalogs.
/// 4. Direct download, zip decompression and injection into playback engine.
class SubtitleSearchService : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool searching READ searching NOTIFY searchingChanged)
    Q_PROPERTY(bool downloading READ downloading NOTIFY downloadingChanged)
    Q_PROPERTY(QVariantList results READ results NOTIFY resultsChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString fileHash READ fileHash NOTIFY fileHashChanged)
    Q_PROPERTY(QString currentQuery READ currentQuery NOTIFY currentQueryChanged)
    Q_PROPERTY(QString currentLanguage READ currentLanguage NOTIFY currentLanguageChanged)

public:
    explicit SubtitleSearchService(QObject *parent = nullptr);
    ~SubtitleSearchService() override;

    bool searching() const { return m_searching; }
    bool downloading() const { return m_downloading; }
    QVariantList results() const { return m_results; }
    QString statusMessage() const { return m_statusMessage; }
    QString fileHash() const { return m_fileHash; }
    QString currentQuery() const { return m_currentQuery; }
    QString currentLanguage() const { return m_currentLanguage; }

    /// Computes the 64-bit OpenSubtitles checksum for the given video file.
    /// Returns 16 lowercase hex characters (e.g. "8e245d9679d31e12"), or empty on error.
    static QString computeOpenSubtitlesHash(const QString &filePath);
    static QString computeOpenSubtitlesHash(QIODevice *device, qint64 fileSize);

    /// Strips release group tags, resolution, codecs and noise from a filename
    /// to extract a clean title suitable for subtitle searching.
    static QString cleanMediaTitle(const QString &fileName);

    /// Parses OpenSubtitles v1 API JSON payload into structured QVariantList.
    static QVariantList parseOpenSubtitlesJson(const QByteArray &jsonData, bool isExactMatch = false);

    /// Parses Subdl API JSON payload into structured QVariantList.
    static QVariantList parseSubdlJson(const QByteArray &jsonData);

    /// Searches subtitles matching the currently playing media file (using file hash & clean title).
    Q_INVOKABLE void searchForCurrentFile(const QString &filePath);

    /// Performs manual query search with optional language filter ("all", "fa", "en", etc.).
    Q_INVOKABLE void searchByQuery(const QString &query, const QString &language = QStringLiteral("all"));

    /// Downloads the subtitle result at index and prepares it for playback.
    Q_INVOKABLE void downloadAndApply(int resultIndex);

    /// Aborts any active search or download request.
    Q_INVOKABLE void cancelSearch();

signals:
    void searchingChanged();
    void downloadingChanged();
    void resultsChanged();
    void statusMessageChanged();
    void fileHashChanged();
    void currentQueryChanged();
    void currentLanguageChanged();
    void subtitleReadyToApply(const QString &localFilePath);
    void searchFailed(const QString &errorMessage);

private slots:
    void onSearchReplyFinished();
    void onDownloadReplyFinished();

private:
    void executeSearch(const QString &hash, const QString &query, const QString &language);
    void setSearching(bool s);
    void setDownloading(bool d);
    void setStatus(const QString &msg);

    QNetworkAccessManager m_nam;
    QNetworkReply *m_activeSearchReply = nullptr;
    QNetworkReply *m_activeDownloadReply = nullptr;

    bool m_searching = false;
    bool m_downloading = false;
    QVariantList m_results;
    QString m_statusMessage;
    QString m_fileHash;
    QString m_currentQuery;
    QString m_currentLanguage = QStringLiteral("all");
    QString m_currentFilePath;
    int m_downloadingIndex = -1;
};
