#pragma once

#include <QObject>
#include <QString>
#include <QImage>

struct mpv_handle;

namespace parvaz {

class ScreenshotController : public QObject {
    Q_OBJECT

public:
    explicit ScreenshotController(mpv_handle *mpv = nullptr, QObject *parent = nullptr);
    ~ScreenshotController() override = default;

    void setHandle(mpv_handle *mpv) { m_mpv = mpv; }

    /// Captures the current frame as a QImage using mpv's "screenshot-raw".
    /// Returns a null QImage if capture fails.
    QImage captureRawFrame();

    /// Copies the current frame to the system clipboard.
    /// Returns true on success.
    bool copyToClipboard();

    /// Saves the current frame to the specified directory (defaults to ~/Pictures/Parvaz).
    /// Returns the absolute path of the saved file, or empty QString on failure.
    QString saveToFile(const QString &targetDir = QString());

signals:
    void screenshotTaken(const QString &path);
    void screenshotCopied();
    void screenshotFailed(const QString &reason);

private:
    mpv_handle *m_mpv = nullptr;
};

} // namespace parvaz
