#include "playback/ScreenshotController.h"

#include <mpv/client.h>
#include <QGuiApplication>
#include <QClipboard>
#include <QStandardPaths>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <cstring>
#include <cstdio>

namespace parvaz {

ScreenshotController::ScreenshotController(mpv_handle *mpv, QObject *parent)
    : QObject(parent)
    , m_mpv(mpv)
{
}

QImage ScreenshotController::captureRawFrame()
{
    if (!m_mpv) {
        emit screenshotFailed(QStringLiteral("mpv handle is null"));
        return QImage();
    }

    const char *args[] = {"screenshot-raw", "subtitles", "bgr0", nullptr};
    mpv_node res;
    int err = mpv_command_ret(m_mpv, args, &res);
    if (err < 0) {
        std::fprintf(stderr, "parvaz screenshot: command failed err=%d(%s)\n",
                     err, mpv_error_string(err));
        emit screenshotFailed(QStringLiteral("mpv command failed: %1").arg(err));
        return QImage();
    }

    if (res.format != MPV_FORMAT_NODE_MAP || !res.u.list) {
        mpv_free_node_contents(&res);
        emit screenshotFailed(QStringLiteral("Invalid mpv_node format received"));
        return QImage();
    }

    int w = 0;
    int h = 0;
    int stride = 0;
    const uchar *data = nullptr;

    for (int i = 0; i < res.u.list->num; ++i) {
        const char *k = res.u.list->keys[i];
        if (!k) continue;

        if (std::strcmp(k, "w") == 0) {
            w = static_cast<int>(res.u.list->values[i].u.int64);
        } else if (std::strcmp(k, "h") == 0) {
            h = static_cast<int>(res.u.list->values[i].u.int64);
        } else if (std::strcmp(k, "stride") == 0) {
            stride = static_cast<int>(res.u.list->values[i].u.int64);
        } else if (std::strcmp(k, "data") == 0 && res.u.list->values[i].format == MPV_FORMAT_BYTE_ARRAY) {
            data = reinterpret_cast<const uchar *>(res.u.list->values[i].u.ba->data);
        }
    }

    if (w <= 0 || h <= 0 || stride <= 0 || !data) {
        std::fprintf(stderr, "parvaz screenshot: frame node w=%d h=%d stride=%d data=%p\n",
                     w, h, stride, reinterpret_cast<const void *>(data));
        mpv_free_node_contents(&res);
        emit screenshotFailed(QStringLiteral("Missing image frame data"));
        return QImage();
    }

    // bgr0 matches QImage::Format_RGB32 on Little-Endian systems (B, G, R, 0xFF).
    // Make a deep copy with .copy() before freeing the mpv_node.
    QImage wrapper(data, w, h, stride, QImage::Format_RGB32);
    QImage frame = wrapper.copy();

    mpv_free_node_contents(&res);
    return frame;
}

bool ScreenshotController::copyToClipboard()
{
    QImage img = captureRawFrame();
    if (img.isNull())
        return false;

    QClipboard *cb = QGuiApplication::clipboard();
    if (!cb) {
        emit screenshotFailed(QStringLiteral("Clipboard not available"));
        return false;
    }

    cb->setImage(img, QClipboard::Clipboard);
    emit screenshotCopied();
    return true;
}

QString ScreenshotController::saveToFile(const QString &targetDir)
{
    QImage img = captureRawFrame();
    if (img.isNull())
        return QString();

    QString dir = targetDir;
    if (dir.isEmpty()) {
        const QString pics = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        dir = pics.isEmpty() ? (QDir::homePath() + QStringLiteral("/Pictures/Parvaz"))
                             : (pics + QStringLiteral("/Parvaz"));
    }

    QDir().mkpath(dir);

    const QString timeStr = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_hh-mm-ss-zzz"));
    const QString filePath = QStringLiteral("%1/Parvaz_%2.png").arg(dir, timeStr);

    if (!img.save(filePath, "PNG")) {
        emit screenshotFailed(QStringLiteral("Failed to write image file"));
        return QString();
    }

    emit screenshotTaken(filePath);
    return filePath;
}

} // namespace parvaz
