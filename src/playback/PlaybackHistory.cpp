#include "playback/PlaybackHistory.h"
#include "mpv/MpvObject.h"

#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QCoreApplication>
#include <cstdio>

namespace parvaz {

PlaybackHistory::PlaybackHistory(MpvObject *mpv, const QString &dbPath, QObject *parent)
    : QObject(parent)
    , m_mpv(mpv)
{
    initDatabase(dbPath);

    if (m_mpv) {
        m_mpv->setHistory(this);
        connect(m_mpv, &MpvObject::fileLoaded, this, &PlaybackHistory::onFileLoaded);
    }

    if (auto *app = QCoreApplication::instance()) {
        connect(app, &QCoreApplication::aboutToQuit, this, &PlaybackHistory::saveCurrentState);
    }

    connect(&m_saveTimer, &QTimer::timeout, this, &PlaybackHistory::onPeriodicSave);
    m_saveTimer.start(5000);
}

PlaybackHistory::~PlaybackHistory()
{
    saveCurrentState();
    m_saveTimer.stop();

    if (m_mpv) {
        m_mpv->setHistory(nullptr);
    }

    if (!m_connectionName.isEmpty()) {
        {
            QSqlDatabase db = QSqlDatabase::database(m_connectionName);
            if (db.isOpen())
                db.close();
        }
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

void PlaybackHistory::initDatabase(const QString &dbPath)
{
    QString targetPath = dbPath;
    if (targetPath.isEmpty()) {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(dir);
        targetPath = dir + QStringLiteral("/state.db");
    } else {
        const QFileInfo info(targetPath);
        QDir().mkpath(info.absolutePath());
    }

    m_connectionName = QStringLiteral("parvaz_history_")
                       + QString::number(reinterpret_cast<quintptr>(this));

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(targetPath);

    if (!db.open()) {
        std::fprintf(stderr, "parvaz: failed to open history db %s: %s\n",
                     qPrintable(targetPath), qPrintable(db.lastError().text()));
        return;
    }

    QSqlQuery q(db);
    const QString schema = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS playback_history ("
        "  path TEXT PRIMARY KEY,"
        "  position REAL NOT NULL,"
        "  duration REAL NOT NULL,"
        "  updated_at INTEGER NOT NULL"
        ");"
    );

    if (!q.exec(schema)) {
        std::fprintf(stderr, "parvaz: failed to create history table: %s\n",
                     qPrintable(q.lastError().text()));
    }

    const QString bookmarkSchema = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS bookmarks ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  path TEXT NOT NULL,"
        "  position REAL NOT NULL,"
        "  title TEXT,"
        "  created_at INTEGER NOT NULL"
        ");"
    );

    if (!q.exec(bookmarkSchema)) {
        std::fprintf(stderr, "parvaz: failed to create bookmarks table: %s\n",
                     qPrintable(q.lastError().text()));
    }

    q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_bookmarks_path ON bookmarks(path);"));
}

double PlaybackHistory::resumePosition(const QString &filePath) const
{
    if (filePath.isEmpty() || m_connectionName.isEmpty())
        return 0.0;

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return 0.0;

    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT position, duration FROM playback_history WHERE path = :path"));
    q.bindValue(QStringLiteral(":path"), filePath);

    if (q.exec() && q.next()) {
        const double pos = q.value(0).toDouble();
        const double dur = q.value(1).toDouble();
        if (pos >= kMinResumeSeconds && (dur <= 0.0 || pos < (dur - kEndMarginSeconds))) {
            return pos;
        }
    }
    return 0.0;
}

void PlaybackHistory::saveCurrentState()
{
    if (!m_mpv || m_connectionName.isEmpty())
        return;

    const QString path = m_mpv->currentFile();
    if (path.isEmpty())
        return;

    const double pos = m_mpv->position();
    const double dur = m_mpv->duration();

    // If near the end, clear the saved state so replay starts from the beginning.
    if (dur > 0.0 && pos >= (dur - kEndMarginSeconds)) {
        clearState(path);
        return;
    }

    if (pos < kMinResumeSeconds)
        return;

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return;

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO playback_history (path, position, duration, updated_at) "
        "VALUES (:path, :pos, :dur, :ts) "
        "ON CONFLICT(path) DO UPDATE SET "
        "  position = excluded.position, "
        "  duration = excluded.duration, "
        "  updated_at = excluded.updated_at"
    ));
    q.bindValue(QStringLiteral(":path"), path);
    q.bindValue(QStringLiteral(":pos"), pos);
    q.bindValue(QStringLiteral(":dur"), dur);
    q.bindValue(QStringLiteral(":ts"), QDateTime::currentSecsSinceEpoch());

    if (!q.exec()) {
        std::fprintf(stderr, "parvaz: failed to save playback history: %s\n",
                     qPrintable(q.lastError().text()));
    }
}

void PlaybackHistory::clearState(const QString &filePath)
{
    if (filePath.isEmpty() || m_connectionName.isEmpty())
        return;

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return;

    QSqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM playback_history WHERE path = :path"));
    q.bindValue(QStringLiteral(":path"), filePath);
    q.exec();
}

void PlaybackHistory::onFileLoaded()
{
    if (!m_mpv)
        return;

    const QString path = m_mpv->currentFile();
    if (path.isEmpty() || path == m_lastResumedPath)
        return;

    const double pos = resumePosition(path);
    if (pos >= kMinResumeSeconds) {
        m_lastResumedPath = path;
        m_mpv->seekAbsolute(pos);
        m_mpv->showOsd(QStringLiteral("Resumed at %1").arg(m_mpv->formatTime(pos)));
    }
}

void PlaybackHistory::onPeriodicSave()
{
    if (!m_mpv || m_mpv->paused())
        return;

    saveCurrentState();
}

bool PlaybackHistory::addBookmark(const QString &filePath, double position, const QString &title)
{
    if (filePath.isEmpty() || position < 0.0 || m_connectionName.isEmpty())
        return false;

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return false;

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO bookmarks (path, position, title, created_at) "
        "VALUES (:path, :pos, :title, :time)"
    ));
    q.bindValue(QStringLiteral(":path"), filePath);
    q.bindValue(QStringLiteral(":pos"), position);
    q.bindValue(QStringLiteral(":title"), title.isEmpty() ? QStringLiteral("Bookmark") : title);
    q.bindValue(QStringLiteral(":time"), QDateTime::currentSecsSinceEpoch());

    if (!q.exec()) {
        std::fprintf(stderr, "parvaz: failed to insert bookmark: %s\n",
                     qPrintable(q.lastError().text()));
        return false;
    }
    return true;
}

bool PlaybackHistory::removeBookmark(int bookmarkId)
{
    if (bookmarkId <= 0 || m_connectionName.isEmpty())
        return false;

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return false;

    QSqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM bookmarks WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), bookmarkId);
    return q.exec();
}

bool PlaybackHistory::clearBookmarks(const QString &filePath)
{
    if (filePath.isEmpty() || m_connectionName.isEmpty())
        return false;

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return false;

    QSqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM bookmarks WHERE path = :path"));
    q.bindValue(QStringLiteral(":path"), filePath);
    return q.exec();
}

QVariantList PlaybackHistory::bookmarksForFile(const QString &filePath) const
{
    QVariantList list;
    if (filePath.isEmpty() || m_connectionName.isEmpty())
        return list;

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return list;

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT id, position, title, created_at FROM bookmarks WHERE path = :path ORDER BY position ASC"
    ));
    q.bindValue(QStringLiteral(":path"), filePath);

    if (q.exec()) {
        while (q.next()) {
            QVariantMap item;
            item.insert(QStringLiteral("id"), q.value(0).toInt());
            item.insert(QStringLiteral("position"), q.value(1).toDouble());
            item.insert(QStringLiteral("title"), q.value(2).toString());
            item.insert(QStringLiteral("createdAt"), q.value(3).toLongLong());
            list.append(item);
        }
    }
    return list;
}

} // namespace parvaz
