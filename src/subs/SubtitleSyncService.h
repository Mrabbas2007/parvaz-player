#pragma once

#include <QObject>
#include <QString>
#include <QProcess>

/// Automatic subtitle synchronization service using ffsubsync.
///
/// Detects system ffsubsync installation, runs asynchronous audio-to-text
/// alignment, and delivers the resynchronized subtitle path to the player.
class SubtitleSyncService : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool syncing READ syncing NOTIFY syncingChanged)
    Q_PROPERTY(bool isToolAvailable READ isToolAvailable NOTIFY isToolAvailableChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

public:
    explicit SubtitleSyncService(QObject *parent = nullptr);
    ~SubtitleSyncService() override;

    bool syncing() const { return m_syncing; }
    bool isToolAvailable() const;
    QString statusMessage() const { return m_statusMessage; }

    /// Starts asynchronous synchronization of subtitlePath against videoPath.
    Q_INVOKABLE void syncSubtitle(const QString &videoPath, const QString &subtitlePath);

    /// Cancels active synchronization process if running.
    Q_INVOKABLE void cancelSync();

signals:
    void syncingChanged();
    void isToolAvailableChanged();
    void statusMessageChanged();
    void syncFinished(const QString &syncedSubtitlePath);
    void syncFailed(const QString &errorMessage);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);

private:
    void setSyncing(bool s);
    void setStatus(const QString &msg);

    QProcess *m_process = nullptr;
    bool m_syncing = false;
    QString m_statusMessage;
    QString m_targetOutputPath;
};
