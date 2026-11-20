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

QVariantList SubtitleSearchService::parseStremioSubtitlesJson(const QByteArray &jsonData, const QString &sourceName)
{
    QVariantList list;
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return list;

    QJsonArray subsArr = doc.object().value(QStringLiteral("subtitles")).toArray();
    for (const QJsonValue &val : subsArr) {
        if (!val.isObject())
            continue;
        QJsonObject obj = val.toObject();

        QVariantMap item;
        item.insert(QStringLiteral("id"), obj.value(QStringLiteral("id")).toVariant());

        QString title = obj.value(QStringLiteral("title")).toString();
        if (title.isEmpty())
            title = obj.value(QStringLiteral("subtitleFileName")).toString();
        if (title.isEmpty())
            title = obj.value(QStringLiteral("movieReleaseName")).toString();

        item.insert(QStringLiteral("release"), title.isEmpty() ? QStringLiteral("Subtitle Track") : title);

        QString rawLang = obj.value(QStringLiteral("lang")).toString().toLower();
        QString langCode;
        QString langName;

        if (rawLang == QLatin1String("fas") || rawLang == QLatin1String("fa") || rawLang == QLatin1String("per")) {
            langCode = QStringLiteral("fa");
            langName = QStringLiteral("Persian (FA)");
        } else if (rawLang == QLatin1String("eng") || rawLang == QLatin1String("en")) {
            langCode = QStringLiteral("en");
            langName = QStringLiteral("English (EN)");
        } else {
            langCode = rawLang;
            langName = rawLang.toUpper();
        }

        item.insert(QStringLiteral("langCode"), langCode);
        item.insert(QStringLiteral("language"), langName);
        item.insert(QStringLiteral("downloads"), obj.value(QStringLiteral("downloads")).toInt());
        item.insert(QStringLiteral("rating"), 0.0);
        item.insert(QStringLiteral("hearingImpaired"), false);
        item.insert(QStringLiteral("isExactMatch"), false);
        item.insert(QStringLiteral("source"), sourceName);
        item.insert(QStringLiteral("downloadUrl"), obj.value(QStringLiteral("url")).toString());

        if (!item.value(QStringLiteral("downloadUrl")).toString().isEmpty()) {
            list.append(item);
        }
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
    bool languageChanged = (m_currentLanguage != language);
    m_currentLanguage = language;
    emit currentLanguageChanged();

    if (!m_rawSearchResults.isEmpty() && m_currentQuery == query && languageChanged) {
        applyLanguageFilter();
        return;
    }

    m_currentQuery = query;
    emit currentQueryChanged();

    executeSearch(QString(), query, language);
}

void SubtitleSearchService::executeSearch(const QString &hash, const QString &query, const QString &language)
{
    Q_UNUSED(hash);
    Q_UNUSED(language);
    cancelSearch();
    m_appliedIndex = -1;
    emit appliedIndexChanged();
    setSearching(true);
    setStatus(QStringLiteral("Searching subtitles online…"));
    m_rawSearchResults.clear();
    m_results.clear();
    emit resultsChanged();

    // If query is an explicit IMDb ID: tt1234567 or tt1234567:1:2
    static const QRegularExpression imdbRegex(QStringLiteral("^tt\\d+(:\\d+:\\d+)?$"));
    if (imdbRegex.match(query.trimmed()).hasMatch()) {
        QString fullId = query.trimmed();
        QString type = fullId.contains(QLatin1Char(':')) ? QStringLiteral("series") : QStringLiteral("movie");
        fetchSubtitlesForImdb(type, fullId, 0, 0);
        return;
    }

    QString target = query.trimmed();
    if (target.isEmpty() && !m_currentFilePath.isEmpty()) {
        target = cleanMediaTitle(QFileInfo(m_currentFilePath).fileName());
    }

    if (target.isEmpty()) {
        setSearching(false);
        setStatus(QStringLiteral("Please enter a title to search"));
        return;
    }

    int season = -1;
    int episode = -1;
    QString cleanTitle = target;

    // 1. Detect TV season and episode: S01E02 or 1x02
    static const QRegularExpression tvPattern(QStringLiteral("(?i)\\b(?:s(\\d{1,3})[\\s._-]*e(\\d{1,4})|(\\d{1,2})x(\\d{1,4}))\\b"));
    QRegularExpressionMatch tvMatch = tvPattern.match(target);
    if (tvMatch.hasMatch()) {
        season = tvMatch.captured(1).isEmpty() ? tvMatch.captured(3).toInt() : tvMatch.captured(1).toInt();
        episode = tvMatch.captured(2).isEmpty() ? tvMatch.captured(4).toInt() : tvMatch.captured(2).toInt();
        cleanTitle = target.left(tvMatch.capturedStart()).trimmed();
    } else {
        // 2. Detect Anime dash-episode pattern: "Title - 17"
        static const QRegularExpression animeDash(QStringLiteral("^(.*?)\\s*-\\s*(\\d{1,4})(?:v\\d+)?(?:\\b|[.\\s_\\[\\(-]|$)"));
        QRegularExpressionMatch aMatch = animeDash.match(target);
        if (aMatch.hasMatch()) {
            int ep = aMatch.captured(2).toInt();
            if (ep > 0 && (ep < 1900 || ep > 2100)) {
                season = 1;
                episode = ep;
                cleanTitle = aMatch.captured(1).trimmed();
            }
        }
    }

    cleanTitle.remove(QRegularExpression(QStringLiteral("^\\[[^\\]]*\\]\\s*")));
    cleanTitle.replace(QLatin1Char('.'), QLatin1Char(' '));
    cleanTitle.replace(QLatin1Char('_'), QLatin1Char(' '));
    cleanTitle = cleanTitle.simplified();

    QString catalogType = (season > 0 && episode > 0) ? QStringLiteral("series") : QStringLiteral("movie");

    // Query Cinemeta catalog to resolve Title to IMDb ID
    QUrl cinemetaUrl(QStringLiteral("https://v3-cinemeta.strem.io/catalog/%1/top/search=%2.json")
                         .arg(catalogType, QString::fromUtf8(QUrl::toPercentEncoding(cleanTitle))));

    QNetworkRequest req(cinemetaUrl);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0 (X11; Linux x86_64)"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = m_nam.get(req);
    m_pendingSearchReplies.append(reply);

    connect(reply, &QNetworkReply::finished, this, [this, reply, catalogType, cleanTitle, season, episode]() {
        m_pendingSearchReplies.removeOne(reply);
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            std::fprintf(stderr, "parvaz: cinemeta search error: %s\n", qPrintable(reply->errorString()));
            setSearching(false);
            setStatus(QStringLiteral("Unable to reach subtitle server. Check your connection."));
            return;
        }

        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonArray metas = doc.object().value(QStringLiteral("metas")).toArray();

        if (metas.isEmpty()) {
            // Fallback between series and movie
            QString fallbackType = (catalogType == QLatin1String("series")) ? QStringLiteral("movie") : QStringLiteral("series");
            QUrl fallbackUrl(QStringLiteral("https://v3-cinemeta.strem.io/catalog/%1/top/search=%2.json")
                                 .arg(fallbackType, QString::fromUtf8(QUrl::toPercentEncoding(cleanTitle))));
            QNetworkRequest fReq(fallbackUrl);
            fReq.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0 (X11; Linux x86_64)"));
            fReq.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

            QNetworkReply *fReply = m_nam.get(fReq);
            m_pendingSearchReplies.append(fReply);
            connect(fReply, &QNetworkReply::finished, this, [this, fReply, fallbackType, season, episode]() {
                m_pendingSearchReplies.removeOne(fReply);
                fReply->deleteLater();
                if (fReply->error() == QNetworkReply::NoError) {
                    QJsonDocument fDoc = QJsonDocument::fromJson(fReply->readAll());
                    QJsonArray fMetas = fDoc.object().value(QStringLiteral("metas")).toArray();
                    if (!fMetas.isEmpty()) {
                        QString imdbId = fMetas.first().toObject().value(QStringLiteral("id")).toString();
                        fetchSubtitlesForImdb(fallbackType, imdbId, season, episode);
                        return;
                    }
                }
                setSearching(false);
                setStatus(QStringLiteral("No subtitles found for this title"));
            });
            return;
        }

        QString imdbId = metas.first().toObject().value(QStringLiteral("id")).toString();
        fetchSubtitlesForImdb(catalogType, imdbId, season, episode);
    });
}

void SubtitleSearchService::fetchSubtitlesForImdb(const QString &mediaType, const QString &imdbId, int season, int episode)
{
    QString queryId = imdbId;
    if (mediaType == QLatin1String("series") && season > 0 && episode > 0 && !queryId.contains(QLatin1Char(':'))) {
        queryId = QStringLiteral("%1:%2:%3").arg(imdbId).arg(season).arg(episode);
    }

    m_pendingRepliesCount = 0;

    // 1. OpenSubtitles v3 Mirror (Global / English / Multi)
    QUrl osUrl(QStringLiteral("https://opensubtitles-v3.strem.io/subtitles/%1/%2.json").arg(mediaType, queryId));
    QNetworkRequest osReq(osUrl);
    osReq.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0 (X11; Linux x86_64)"));
    osReq.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *osReply = m_nam.get(osReq);
    m_pendingSearchReplies.append(osReply);
    m_pendingRepliesCount++;

    connect(osReply, &QNetworkReply::finished, this, [this, osReply]() {
        m_pendingSearchReplies.removeOne(osReply);
        osReply->deleteLater();
        if (osReply->error() == QNetworkReply::NoError) {
            QVariantList osList = parseStremioSubtitlesJson(osReply->readAll(), QStringLiteral("OpenSubtitles"));
            m_rawSearchResults.append(osList);
        }
        m_pendingRepliesCount--;
        if (m_pendingRepliesCount <= 0) {
            setSearching(false);
            applyLanguageFilter();
        }
    });

    // 2. Persian SubSource / Stremio Mirror
    QUrl faUrl(QStringLiteral("https://stremio.alirostami.com/subtitles/%1/%2.json").arg(mediaType, queryId));
    QNetworkRequest faReq(faUrl);
    faReq.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0 (X11; Linux x86_64)"));
    faReq.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *faReply = m_nam.get(faReq);
    m_pendingSearchReplies.append(faReply);
    m_pendingRepliesCount++;

    connect(faReply, &QNetworkReply::finished, this, [this, faReply]() {
        m_pendingSearchReplies.removeOne(faReply);
        faReply->deleteLater();
        if (faReply->error() == QNetworkReply::NoError) {
            QVariantList faList = parseStremioSubtitlesJson(faReply->readAll(), QStringLiteral("SubSource"));
            for (int i = faList.size() - 1; i >= 0; --i) {
                m_rawSearchResults.prepend(faList.at(i));
            }
        }
        m_pendingRepliesCount--;
        if (m_pendingRepliesCount <= 0) {
            setSearching(false);
            applyLanguageFilter();
        }
    });
}

void SubtitleSearchService::applyLanguageFilter()
{
    QVariantList filtered;
    int faCount = 0;
    int enCount = 0;

    for (const QVariant &v : m_rawSearchResults) {
        QVariantMap item = v.toMap();
        QString lang = item.value(QStringLiteral("langCode")).toString();
        if (lang == QLatin1String("fa"))
            faCount++;
        else if (lang == QLatin1String("en"))
            enCount++;

        if (m_currentLanguage == QLatin1String("all")) {
            filtered.append(item);
        } else if (m_currentLanguage == lang) {
            filtered.append(item);
        }
    }

    m_results = filtered;
    emit resultsChanged();

    if (m_rawSearchResults.isEmpty()) {
        setStatus(QStringLiteral("No subtitles found for this title"));
    } else if (filtered.isEmpty()) {
        setStatus(QStringLiteral("No %1 subtitles found (%2 total available)")
                      .arg(m_currentLanguage == QLatin1String("fa") ? QStringLiteral("Persian") : QStringLiteral("English"),
                           QString::number(m_rawSearchResults.size())));
    } else {
        setStatus(QStringLiteral("Found %1 subtitle(s) (%2 Persian, %3 English)")
                      .arg(filtered.size())
                      .arg(faCount)
                      .arg(enCount));
    }
}

void SubtitleSearchService::onSearchReplyFinished()
{
}

void SubtitleSearchService::downloadAndApply(int resultIndex)
{
    if (resultIndex < 0 || resultIndex >= m_results.size())
        return;

    if (m_downloading)
        return;

    m_downloadingIndex = resultIndex;
    emit downloadingIndexChanged();
    setDownloading(true);
    setStatus(QStringLiteral("Downloading subtitle…"));

    QVariantMap item = m_results.at(resultIndex).toMap();
    QString downloadUrl = item.value(QStringLiteral("downloadUrl")).toString();

    if (downloadUrl.isEmpty()) {
        m_downloadingIndex = -1;
        emit downloadingIndexChanged();
        setDownloading(false);
        setStatus(QStringLiteral("Download URL not available for this item"));
        return;
    }

    QNetworkRequest request((QUrl(downloadUrl)));
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0 (X11; Linux x86_64)"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

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
    const int completedIndex = m_downloadingIndex;
    m_downloadingIndex = -1;
    emit downloadingIndexChanged();
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

    QVariantMap item = (completedIndex >= 0 && completedIndex < m_results.size())
                           ? m_results.at(completedIndex).toMap()
                           : QVariantMap();

    QString subId = item.value(QStringLiteral("id")).toString();
    subId.replace(QRegularExpression(QStringLiteral("[^a-zA-Z0-9_-]")), QStringLiteral("_"));
    if (subId.isEmpty())
        subId = QString::number(QDateTime::currentMSecsSinceEpoch());

    QString releaseName = item.value(QStringLiteral("release")).toString();
    if (releaseName.isEmpty())
        releaseName = QStringLiteral("subtitle");

    // Clean up filename: keep alphanumeric, dot, underscore, dash
    QString safeFileName = releaseName;
    safeFileName.replace(QRegularExpression(QStringLiteral("[^a-zA-Z0-9._-]")), QStringLiteral("_"));
    safeFileName.replace(QRegularExpression(QStringLiteral("_+")), QStringLiteral("_"));
    safeFileName = safeFileName.trimmed();
    while (safeFileName.startsWith(QLatin1Char('_')) || safeFileName.startsWith(QLatin1Char('.'))) {
        safeFileName.remove(0, 1);
    }
    // Limit base filename to 60 chars so total filename is well under Linux NAME_MAX (255 bytes)
    if (safeFileName.length() > 60) {
        safeFileName = safeFileName.left(60);
    }
    while (safeFileName.endsWith(QLatin1Char('_')) || safeFileName.endsWith(QLatin1Char('.'))) {
        safeFileName.chop(1);
    }
    if (safeFileName.isEmpty()) {
        safeFileName = QStringLiteral("sub");
    }
    safeFileName = QStringLiteral("%1_%2").arg(safeFileName, subId);

    // Check if zip format (magic bytes: PK\x03\x04)
    bool isZip = data.startsWith("PK\x03\x04");
    QString finalSubtitlePath;

    if (isZip) {
        QString zipPath = cacheDir + QStringLiteral("/") + safeFileName + QStringLiteral(".zip");
        QFile zipFile(zipPath);
        if (zipFile.open(QIODevice::WriteOnly)) {
            zipFile.write(data);
            zipFile.close();

            // Extract with unzip into an isolated subdirectory to avoid picking old files.
            // Windows ships no `unzip` binary; fall back to 7z (7z.exe) before failing.
            QString extractDir = cacheDir + QStringLiteral("/zip_") + safeFileName;
            QDir().mkpath(extractDir);

            QProcess extractor;
            bool extracted = false;
            const QString unzipBin = QStandardPaths::findExecutable(QStringLiteral("unzip"));
            if (!unzipBin.isEmpty()) {
                extractor.start(unzipBin, {QStringLiteral("-o"), zipPath, QStringLiteral("-d"), extractDir});
                extracted = true;
            } else {
                const QString sevenZipBin = QStandardPaths::findExecutable(QStringLiteral("7z"));
                if (!sevenZipBin.isEmpty()) {
                    extractor.start(sevenZipBin, {QStringLiteral("x"), zipPath, QStringLiteral("-o") + extractDir});
                    extracted = true;
                }
            }
            if (extracted && extractor.waitForFinished(5000) && extractor.exitCode() == 0) {
                // Find extracted .srt or .ass or .vtt file
                QDir dir(extractDir);
                QStringList entries = dir.entryList({QStringLiteral("*.srt"), QStringLiteral("*.ass"), QStringLiteral("*.vtt")}, QDir::Files, QDir::Time);
                if (!entries.isEmpty()) {
                    finalSubtitlePath = dir.filePath(entries.first());
                }
            }
        }
    }

    if (finalSubtitlePath.isEmpty()) {
        // Detect extension from content
        QString ext = QStringLiteral(".srt");
        if (data.startsWith("WEBVTT")) {
            ext = QStringLiteral(".vtt");
        } else if (data.contains("[Script Info]")) {
            ext = QStringLiteral(".ass");
        }

        finalSubtitlePath = cacheDir + QStringLiteral("/") + safeFileName + ext;
        QFile file(finalSubtitlePath);
        if (!file.open(QIODevice::WriteOnly)) {
            std::fprintf(stderr, "parvaz: failed to open subtitle file for write: %s (error: %s)\n",
                         qPrintable(finalSubtitlePath), qPrintable(file.errorString()));
            setStatus(QStringLiteral("Failed to write subtitle file to disk"));
            return;
        }
        file.write(data);
        file.close();
    }

    setStatus(QStringLiteral("Subtitle downloaded successfully!"));
    m_appliedIndex = completedIndex;
    emit appliedIndexChanged();
    emit subtitleReadyToApply(finalSubtitlePath);
}

void SubtitleSearchService::cancelSearch()
{
    for (QNetworkReply *rep : m_pendingSearchReplies) {
        rep->abort();
        rep->deleteLater();
    }
    m_pendingSearchReplies.clear();
    m_pendingRepliesCount = 0;

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
    if (m_downloadingIndex != -1) {
        m_downloadingIndex = -1;
        emit downloadingIndexChanged();
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
