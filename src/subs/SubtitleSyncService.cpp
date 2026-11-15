#include "subs/SubtitleSyncService.h"

#include <QFileInfo>
#include <QStandardPaths>
#include <cstdio>

SubtitleSyncService::SubtitleSyncService(QObject *parent)
    : QObject(parent)
{
    m_process = new QProcess(this);
    connect(m_process, &QProcess::finished, this, &SubtitleSyncService::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &SubtitleSyncService::onProcessError);
}

SubtitleSyncService::~SubtitleSyncService()
{
    cancelSync();
}

bool SubtitleSyncService::isToolAvailable() const
{
    return !QStandardPaths::findExecutable(QStringLiteral("ffsubsync")).isEmpty();
}

void SubtitleSyncService::syncSubtitle(const QString &videoPath, const QString &subtitlePath)
{
    if (m_syncing)
        return;

    if (!isToolAvailable()) {
        setStatus(QStringLiteral("ffsubsync is not installed. Install via: pip install ffsubsync"));
        emit syncFailed(m_statusMessage);
        return;
    }

    if (videoPath.isEmpty() || subtitlePath.isEmpty()) {
        setStatus(QStringLiteral("Invalid video or subtitle path"));
        emit syncFailed(m_statusMessage);
        return;
    }

    QFileInfo subInfo(subtitlePath);
    m_targetOutputPath = subInfo.absolutePath() + QStringLiteral("/") + subInfo.completeBaseName() + QStringLiteral(".synced.srt");

    setSyncing(true);
    setStatus(QStringLiteral("Synchronizing subtitle with audio stream…"));

    QStringList args;
    args << videoPath << QStringLiteral("-i") << subtitlePath << QStringLiteral("-o") << m_targetOutputPath;

    m_process->start(QStringLiteral("ffsubsync"), args);
}

void SubtitleSyncService::cancelSync()
{
    if (m_syncing && m_process) {
        m_process->kill();
        m_process->waitForFinished(1000);
        setSyncing(false);
        setStatus(QStringLiteral("Synchronization cancelled"));
    }
}

void SubtitleSyncService::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    setSyncing(false);
    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        setStatus(QStringLiteral("Subtitle synchronized successfully!"));
        emit syncFinished(m_targetOutputPath);
    } else {
        QString err = QString::fromUtf8(m_process->readAllStandardError());
        if (err.isEmpty())
            err = QStringLiteral("ffsubsync failed with exit code %1").arg(exitCode);
        std::fprintf(stderr, "parvaz: ffsubsync error: %s\n", qPrintable(err));
        setStatus(QStringLiteral("Sync failed: %1").arg(err));
        emit syncFailed(err);
    }
}

void SubtitleSyncService::onProcessError(QProcess::ProcessError error)
{
    if (m_syncing) {
        setSyncing(false);
        QString errStr;
        switch (error) {
        case QProcess::FailedToStart:
            errStr = QStringLiteral("ffsubsync failed to start");
            break;
        case QProcess::Crashed:
            errStr = QStringLiteral("ffsubsync process crashed");
            break;
        case QProcess::Timedout:
            errStr = QStringLiteral("ffsubsync operation timed out");
            break;
        default:
            errStr = QStringLiteral("ffsubsync process error (%1)").arg(error);
            break;
        }
        setStatus(errStr);
        emit syncFailed(errStr);
    }
}

void SubtitleSyncService::setSyncing(bool s)
{
    if (m_syncing != s) {
        m_syncing = s;
        emit syncingChanged();
    }
}

void SubtitleSyncService::setStatus(const QString &msg)
{
    if (m_statusMessage != msg) {
        m_statusMessage = msg;
        emit statusMessageChanged();
    }
}
