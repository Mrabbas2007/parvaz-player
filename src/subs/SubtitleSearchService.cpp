#include "subs/SubtitleSearchService.h"

#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>
#include <QtEndian>

#include <cstdio>

SubtitleSearchService::SubtitleSearchService(QObject *parent)
    : QObject(parent)
{
}

SubtitleSearchService::~SubtitleSearchService()
{
    cancelSearch();
}

QString SubtitleSearchService::computeOpenSubtitlesHash(QIODevice *device, qint64 fileSize)
{
    if (!device || fileSize < 131072) // Requires at least 128 KB
        return QString();

    quint64 hash = static_cast<quint64>(fileSize);
    quint64 buffer[8192]; // 65536 bytes / 8 = 8192 64-bit words

    // 1. Read first 64KB
    if (!device->seek(0))
        return QString();
    if (device->read(reinterpret_cast<char *>(buffer), 65536) != 65536)
        return QString();

    for (int i = 0; i < 8192; ++i) {
        hash += qFromLittleEndian(buffer[i]);
    }

    // 2. Read last 64KB
    if (!device->seek(fileSize - 65536))
        return QString();
    if (device->read(reinterpret_cast<char *>(buffer), 65536) != 65536)
        return QString();

    for (int i = 0; i < 8192; ++i) {
        hash += qFromLittleEndian(buffer[i]);
    }

    return QString::asprintf("%016llx", static_cast<unsigned long long>(hash));
}

QString SubtitleSearchService::computeOpenSubtitlesHash(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return QString();

    return computeOpenSubtitlesHash(&file, file.size());
}

QString SubtitleSearchService::cleanMediaTitle(const QString &fileName)
{
    if (fileName.isEmpty())
        return QString();

    // 1. Strip directory path and file extension
    QString base = QFileInfo(fileName).completeBaseName();

    // 2. Replace dots, underscores and dashes with spaces
    base.replace(QLatin1Char('.'), QLatin1Char(' '));
    base.replace(QLatin1Char('_'), QLatin1Char(' '));

    // 3. Remove bracketed / parenthesized groups like [YTS.MX], (2024), etc.
    base.remove(QRegularExpression(QStringLiteral("\\[[^\\]]*\\]")));

    // 4. Check for Season and Episode pattern: S01E02 or 1x02
    QRegularExpression tvRegex(QStringLiteral("(?i)\\b(?:s(\\d+)\\s*e(\\d+)|(\\d+)x(\\d+))\\b"));
    QRegularExpressionMatch tvMatch = tvRegex.match(base);

    QString tvTag;
    int cutIndex = -1;
    if (tvMatch.hasMatch()) {
        int s = tvMatch.captured(1).isEmpty() ? tvMatch.captured(3).toInt() : tvMatch.captured(1).toInt();
        int e = tvMatch.captured(2).isEmpty() ? tvMatch.captured(4).toInt() : tvMatch.captured(2).toInt();
        tvTag = QString::asprintf("S%02dE%02d", s, e);
        cutIndex = tvMatch.capturedStart();
    }

    // 5. Check for 4-digit Year (1900..2099)
    QRegularExpression yearRegex(QStringLiteral("\\b(19\\d\\d|20\\d\\d)\\b"));
    QRegularExpressionMatch yearMatch = yearRegex.match(base);
    QString yearTag;
    if (yearMatch.hasMatch()) {
        yearTag = yearMatch.captured(1);
        if (cutIndex < 0 || yearMatch.capturedStart() < cutIndex) {
            cutIndex = yearMatch.capturedStart();
        }
    }

    // 6. Check for common scene release keywords to truncate trailing clutter
    QRegularExpression sceneRegex(QStringLiteral("(?i)\\b(1080p|720p|480p|2160p|4k|uhd|bluray|blu-ray|bdrip|brrip|web-dl|webrip|web|hdtv|hdrip|dvdrip|x264|x265|hevc|avc|10bit|aac|dts|ac3|ddp|atmos|remux|repack|yts|yify|rarbg|psa|galaxytv|sparks|eztv|surcode)\\b"));
    QRegularExpressionMatch sceneMatch = sceneRegex.match(base);
    if (sceneMatch.hasMatch()) {
        if (cutIndex < 0 || sceneMatch.capturedStart() < cutIndex) {
            cutIndex = sceneMatch.capturedStart();
        }
    }

    QString titlePart = (cutIndex > 0) ? base.left(cutIndex) : base;
    titlePart = titlePart.simplified();

    if (!tvTag.isEmpty()) {
        return QStringLiteral("%1 %2").arg(titlePart, tvTag).trimmed();
    }
    if (!yearTag.isEmpty() && !titlePart.contains(yearTag)) {
        return QStringLiteral("%1 %2").arg(titlePart, yearTag).trimmed();
    }

    return titlePart.isEmpty() ? base.simplified() : titlePart;
}

QVariantList SubtitleSearchService::parseOpenSubtitlesJson(const QByteArray &jsonData, bool isExactMatch)
{
    QVariantList list;
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return list;

    QJsonArray dataArr = doc.object().value(QStringLiteral("data")).toArray();
    for (const QJsonValue &val : dataArr) {
        if (!val.isObject())
            continue;
        QJsonObject obj = val.toObject();
        QJsonObject attr = obj.value(QStringLiteral("attributes")).toObject();

        QVariantMap item;
        item.insert(QStringLiteral("id"), obj.value(QStringLiteral("id")).toVariant());
        item.insert(QStringLiteral("release"), attr.value(QStringLiteral("release")).toString());
        if (item.value(QStringLiteral("release")).toString().isEmpty()) {
            item.insert(QStringLiteral("release"), attr.value(QStringLiteral("feature_details")).toObject().value(QStringLiteral("movie_name")).toString());
        }

        QString lang = attr.value(QStringLiteral("language")).toString();
        item.insert(QStringLiteral("langCode"), lang);
        item.insert(QStringLiteral("language"), lang == QLatin1String("fa") ? QStringLiteral("Persian (FA)") :
                                                (lang == QLatin1String("en") ? QStringLiteral("English") : lang));

        item.insert(QStringLiteral("downloads"), attr.value(QStringLiteral("download_count")).toInt());
        item.insert(QStringLiteral("rating"), attr.value(QStringLiteral("ratings")).toDouble());
        item.insert(QStringLiteral("hearingImpaired"), attr.value(QStringLiteral("hearing_impaired")).toBool());
        item.insert(QStringLiteral("isExactMatch"), isExactMatch);
        item.insert(QStringLiteral("source"), QStringLiteral("OpenSubtitles"));

        // Files array holds file_id and download details
        QJsonArray files = attr.value(QStringLiteral("files")).toArray();
        if (!files.isEmpty()) {
            QJsonObject firstFile = files.first().toObject();
            item.insert(QStringLiteral("fileId"), firstFile.value(QStringLiteral("file_id")).toInt());
            item.insert(QStringLiteral("fileName"), firstFile.value(QStringLiteral("file_name")).toString());
        }

        // Direct download link or web url
        item.insert(QStringLiteral("downloadUrl"), attr.value(QStringLiteral("url")).toString());
        list.append(item);
    }

    return list;
}

QVariantList SubtitleSearchService::parseSubdlJson(const QByteArray &jsonData)
{
    QVariantList list;
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return list;

    QJsonArray subs = doc.object().value(QStringLiteral("subtitles")).toArray();
    for (const QJsonValue &val : subs) {
        if (!val.isObject())
            continue;
        QJsonObject s = val.toObject();

        QVariantMap item;
        QString release = s.value(QStringLiteral("release_name")).toString();
        if (release.isEmpty())
            release = s.value(QStringLiteral("name")).toString();

        item.insert(QStringLiteral("id"), s.value(QStringLiteral("id")).toVariant());
        item.insert(QStringLiteral("release"), release);

        QString lang = s.value(QStringLiteral("lang")).toString();
        QString langName = s.value(QStringLiteral("language")).toString();
        item.insert(QStringLiteral("langCode"), lang);
        item.insert(QStringLiteral("language"), langName.isEmpty() ? lang : langName);

        item.insert(QStringLiteral("downloads"), s.value(QStringLiteral("download_count")).toInt());
        item.insert(QStringLiteral("rating"), s.value(QStringLiteral("rating")).toDouble());
        item.insert(QStringLiteral("hearingImpaired"), s.value(QStringLiteral("hi")).toBool());
        item.insert(QStringLiteral("isExactMatch"), false);
        item.insert(QStringLiteral("source"), QStringLiteral("Subdl"));

        QString url = s.value(QStringLiteral("url")).toString();
        if (!url.startsWith(QLatin1String("http"))) {
            url = QStringLiteral("https://subdl.com") + url;
        }
        item.insert(QStringLiteral("downloadUrl"), url);
        list.append(item);
    }

    return list;
}

void SubtitleSearchService::searchForCurrentFile(const QString &filePath)
{
    m_currentFilePath = filePath;
    m_fileHash.clear();
    emit fileHashChanged();

    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        setStatus(QStringLiteral("No media file currently open"));
        return;
    }

    // 1. Compute hash
    m_fileHash = computeOpenSubtitlesHash(filePath);
    emit fileHashChanged();

    // 2. Clean title
    QString cleanName = cleanMediaTitle(QFileInfo(filePath).fileName());
    m_currentQuery = cleanName;
    emit currentQueryChanged();

    executeSearch(m_fileHash, cleanName, m_currentLanguage);
}

void SubtitleSearchService::searchByQuery(const QString &query, const QString &language)
{
    m_currentQuery = query;
    emit currentQueryChanged();
    m_currentLanguage = language;
    emit currentLanguageChanged();

    executeSearch(QString(), query, language);
}

void SubtitleSearchService::executeSearch(const QString &hash, const QString &query, const QString &language)
{
    cancelSearch();
    setSearching(true);
    setStatus(QStringLiteral("Searching subtitles online…"));
    m_results.clear();
    emit resultsChanged();

    // Build OpenSubtitles query URL
    QUrl url(QStringLiteral("https://api.opensubtitles.com/api/v1/subtitles"));
    QUrlQuery q;
    if (!hash.isEmpty()) {
        q.addQueryItem(QStringLiteral("moviehash"), hash);
    }
    if (!query.isEmpty()) {
        q.addQueryItem(QStringLiteral("query"), query);
    }
    if (language != QLatin1String("all") && !language.isEmpty()) {
        q.addQueryItem(QStringLiteral("languages"), language);
    } else {
        q.addQueryItem(QStringLiteral("languages"), QStringLiteral("fa,en"));
    }

    url.setQuery(q);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("ParvazPlayer v0.1.0"));
    request.setRawHeader("Accept", "application/json");

    m_activeSearchReply = m_nam.get(request);
    connect(m_activeSearchReply, &QNetworkReply::finished,
            this, &SubtitleSearchService::onSearchReplyFinished);
}

void SubtitleSearchService::onSearchReplyFinished()
{
    if (!m_activeSearchReply)
        return;

    QNetworkReply *reply = m_activeSearchReply;
    m_activeSearchReply = nullptr;
    reply->deleteLater();
    setSearching(false);

    if (reply->error() != QNetworkReply::NoError) {
        QString errStr = reply->errorString();
        std::fprintf(stderr, "parvaz: subtitle search network error: %s\n", qPrintable(errStr));
        setStatus(QStringLiteral("Search failed: %1").arg(errStr));
        emit searchFailed(errStr);
        return;
    }

    QByteArray data = reply->readAll();
    bool isHashMatch = !m_fileHash.isEmpty();
    QVariantList parsed = parseOpenSubtitlesJson(data, isHashMatch);

    if (parsed.isEmpty()) {
        setStatus(QStringLiteral("No subtitles found for this title"));
    } else {
        setStatus(QStringLiteral("Found %1 subtitle(s)").arg(parsed.size()));
    }

    m_results = parsed;
    emit resultsChanged();
}

void SubtitleSearchService::downloadAndApply(int resultIndex)
{
    if (resultIndex < 0 || resultIndex >= m_results.size())
        return;

    if (m_downloading)
        return;

    m_downloadingIndex = resultIndex;
    setDownloading(true);
    setStatus(QStringLiteral("Downloading subtitle…"));

    QVariantMap item = m_results.at(resultIndex).toMap();
    QString downloadUrl = item.value(QStringLiteral("downloadUrl")).toString();

    if (downloadUrl.isEmpty()) {
        setDownloading(false);
        setStatus(QStringLiteral("Download URL not available for this item"));
        return;
    }

    QNetworkRequest request((QUrl(downloadUrl)));
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("ParvazPlayer v0.1.0"));

    m_activeDownloadReply = m_nam.get(request);
    connect(m_activeDownloadReply, &QNetworkReply::finished,
            this, &SubtitleSearchService::onDownloadReplyFinished);
}

void SubtitleSearchService::onDownloadReplyFinished()
{
    if (!m_activeDownloadReply)
        return;

    QNetworkReply *reply = m_activeDownloadReply;
    m_activeDownloadReply = nullptr;
    reply->deleteLater();
    setDownloading(false);

    if (reply->error() != QNetworkReply::NoError) {
        QString errStr = reply->errorString();
        std::fprintf(stderr, "parvaz: subtitle download error: %s\n", qPrintable(errStr));
        setStatus(QStringLiteral("Download failed: %1").arg(errStr));
        return;
    }

    QByteArray data = reply->readAll();
    if (data.isEmpty()) {
        setStatus(QStringLiteral("Downloaded empty subtitle file"));
        return;
    }

    // Determine target save directory: ~/.cache/parvaz/subtitles/
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/subtitles");
    QDir().mkpath(cacheDir);

    QVariantMap item = (m_downloadingIndex >= 0 && m_downloadingIndex < m_results.size())
                           ? m_results.at(m_downloadingIndex).toMap()
                           : QVariantMap();

    QString releaseName = item.value(QStringLiteral("release")).toString();
    if (releaseName.isEmpty())
        releaseName = QStringLiteral("subtitle_%1").arg(QDateTime::currentMSecsSinceEpoch());

    QString safeFileName = releaseName;
    safeFileName.replace(QRegularExpression(QStringLiteral("[^a-zA-Z0-9._-]")), QStringLiteral("_"));

    // Check if zip format (magic bytes: PK\x03\x04)
    bool isZip = data.startsWith("PK\x03\x04");
    QString finalSubtitlePath;

    if (isZip) {
        QString zipPath = cacheDir + QStringLiteral("/") + safeFileName + QStringLiteral(".zip");
        QFile zipFile(zipPath);
        if (zipFile.open(QIODevice::WriteOnly)) {
            zipFile.write(data);
            zipFile.close();

            // Extract with unzip
            QProcess unzip;
            unzip.start(QStringLiteral("unzip"), {QStringLiteral("-o"), zipPath, QStringLiteral("-d"), cacheDir});
            if (unzip.waitForFinished(5000) && unzip.exitCode() == 0) {
                // Find extracted .srt or .ass file
                QDir dir(cacheDir);
                QStringList entries = dir.entryList({QStringLiteral("*.srt"), QStringLiteral("*.ass")}, QDir::Files, QDir::Time);
                if (!entries.isEmpty()) {
                    finalSubtitlePath = dir.filePath(entries.first());
                }
            }
        }
    }

    if (finalSubtitlePath.isEmpty()) {
        // Save as raw .srt file
        finalSubtitlePath = cacheDir + QStringLiteral("/") + safeFileName + QStringLiteral(".srt");
        QFile file(finalSubtitlePath);
        if (!file.open(QIODevice::WriteOnly)) {
            setStatus(QStringLiteral("Failed to write subtitle file to disk"));
            return;
        }
        file.write(data);
        file.close();
    }

    setStatus(QStringLiteral("Subtitle downloaded successfully!"));
    emit subtitleReadyToApply(finalSubtitlePath);
}

void SubtitleSearchService::cancelSearch()
{
    if (m_activeSearchReply) {
        m_activeSearchReply->abort();
        m_activeSearchReply->deleteLater();
        m_activeSearchReply = nullptr;
    }
    if (m_activeDownloadReply) {
        m_activeDownloadReply->abort();
        m_activeDownloadReply->deleteLater();
        m_activeDownloadReply = nullptr;
    }
    setSearching(false);
    setDownloading(false);
}

void SubtitleSearchService::setSearching(bool s)
{
    if (m_searching != s) {
        m_searching = s;
        emit searchingChanged();
    }
}

void SubtitleSearchService::setDownloading(bool d)
{
    if (m_downloading != d) {
        m_downloading = d;
        emit downloadingChanged();
    }
}

void SubtitleSearchService::setStatus(const QString &msg)
{
    if (m_statusMessage != msg) {
        m_statusMessage = msg;
        emit statusMessageChanged();
    }
}
