#pragma once

#include <QObject>
#include <QString>
#include <QSqlDatabase>
#include <QTimer>

class MpvObject;

namespace parvaz {

class PlaybackHistory : public QObject {
    Q_OBJECT
public:
    explicit PlaybackHistory(MpvObject *mpv, const QString &dbPath = QString(),
                            QObject *parent = nullptr);
    ~PlaybackHistory() override;

    /// Checks if a file has a saved resume position and returns it in seconds (> 0),
    /// or 0.0 if not found / finished.
    double resumePosition(const QString &filePath) const;

    /// Saves the current playback position for the currently active file.
    void saveCurrentState();

    /// Clears saved position for the given file.
    void clearState(const QString &filePath);

    /// Minimum seconds played before a position is remembered.
    static constexpr double kMinResumeSeconds = 5.0;

    /// Margin from the end: if stopped within this many seconds of the end,
    /// the video is treated as finished and starts from 0 next time.
    static constexpr double kEndMarginSeconds = 15.0;

private slots:
    void onFileLoaded();
    void onPeriodicSave();

private:
    void initDatabase(const QString &dbPath);

    MpvObject *m_mpv = nullptr;
    QString m_connectionName;
    QTimer m_saveTimer;
    QString m_lastResumedPath;
};

} // namespace parvaz
