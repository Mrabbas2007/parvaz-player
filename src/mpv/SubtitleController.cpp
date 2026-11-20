#include "mpv/SubtitleController.h"

#include <QUrl>

#include <cmath>
#include <cstdio>

SubtitleController::SubtitleController(QObject *parent)
    : QObject(parent)
{
}

void SubtitleController::setDelay(double seconds)
{
    if (!m_mpv)
        return;
    double d = seconds;
    mpv_set_property(m_mpv, "sub-delay", MPV_FORMAT_DOUBLE, &d);
}

void SubtitleController::setVisible(bool visible)
{
    if (!m_mpv)
        return;
    // "sid" accepts the string "no" to deselect without unloading the track.
    int flag = visible ? 1 : 0;
    mpv_set_property(m_mpv, "sub-visibility", MPV_FORMAT_FLAG, &flag);
}

void SubtitleController::setPosition(double percentFromTop)
{
    // mpv's documented range is 0..150.
    m_position = qBound(0.0, percentFromTop, 150.0);
    applyPosition();
}

void SubtitleController::setFontSize(double size)
{
    if (!m_mpv)
        return;
    // mpv clamps to 1..9000 itself; keep a sane UI range.
    double s = qBound(8.0, size, 200.0);
    mpv_set_property(m_mpv, "sub-font-size", MPV_FORMAT_DOUBLE, &s);
}

void SubtitleController::setBottomBarOffset(bool visible, double barHeight, double windowHeight)
{
    m_bottomBarVisible = visible;
    m_bottomBarHeight = barHeight;
    m_windowHeight = windowHeight;
    applyPosition();
}

void SubtitleController::applyPosition()
{
    if (!m_mpv)
        return;
    double p = m_position;
    if (m_bottomBarVisible && m_windowHeight > 0.0 && m_bottomBarHeight > 0.0) {
        // Shift subtitle up so it clears the bottom control bar plus a
        // comfortable margin (16px).
        double shiftPercent = ((m_bottomBarHeight + 16.0) / m_windowHeight) * 100.0;
        p = qBound(0.0, p - shiftPercent, 150.0);
    }
    m_lastAppliedPos = p;
    mpv_set_property(m_mpv, "sub-pos", MPV_FORMAT_DOUBLE, &p);
}

void SubtitleController::addSubtitleFile(const QString &path)
{
    if (!m_mpv || path.isEmpty())
        return;
    // The flags argument is a string ("select" = make it current right away).
    const QByteArray encoded = path.toUtf8();
    const char *cmd[] = {"sub-add", encoded.constData(), "select", nullptr};
    if (mpv_command(m_mpv, cmd) < 0)
        std::fprintf(stderr, "parvaz: sub-add failed for %s\n", qPrintable(path));
}

void SubtitleController::setCodepage(const QString &codepage)
{
    if (!m_mpv || codepage.isEmpty())
        return;
    // Without a leading '+', mpv still trusts data that already looks like
    // UTF-8; only "+cp1256" would force the legacy decoder onto UTF-8 files.
    mpv_set_property_string(m_mpv, "sub-codepage", codepage.toUtf8().constData());
    reload();
}

QString SubtitleController::codepage() const
{
    if (!m_mpv)
        return QString();
    char *cp = mpv_get_property_string(m_mpv, "sub-codepage");
    const QString out = cp ? QString::fromUtf8(cp) : QString();
    mpv_free(cp);
    return out;
}

void SubtitleController::reload()
{
    if (!m_mpv)
        return;
    const char *cmd[] = {"sub-reload", nullptr};
    mpv_command(m_mpv, cmd);
}

QString SubtitleController::text() const
{
    if (!m_mpv)
        return QString();
    char *t = mpv_get_property_string(m_mpv, "sub-text");
    const QString out = t ? QString::fromUtf8(t) : QString();
    mpv_free(t);
    return out;
}

void SubtitleController::deselect()
{
    if (!m_mpv)
        return;
    mpv_set_property_string(m_mpv, "sid", "no");
}

void SubtitleController::cycleTrack()
{
    if (!m_mpv)
        return;
    const char *cmd[] = {"cycle", "sid", nullptr};
    mpv_command(m_mpv, cmd);
}

void SubtitleController::cycleSecondaryTrack()
{
    if (!m_mpv)
        return;
    const char *cmd[] = {"cycle", "secondary-sid", nullptr};
    mpv_command_async(m_mpv, 0, cmd);
}

void SubtitleController::selectSecondaryTrack(qint64 id)
{
    if (!m_mpv)
        return;
    if (id <= 0) {
        const char *cmd[] = {"set", "secondary-sid", "no", nullptr};
        mpv_command_async(m_mpv, 0, cmd);
    } else {
        const QByteArray idStr = QByteArray::number(id);
        const char *cmd[] = {"set", "secondary-sid", idStr.constData(), nullptr};
        mpv_command_async(m_mpv, 0, cmd);
    }
}

bool SubtitleController::onMpvProperty(const QString &name, mpv_format format, void *data)
{
    if (name == QLatin1String("sub-delay") && format == MPV_FORMAT_DOUBLE) {
        m_delay = *static_cast<double *>(data);
        emit delayChanged();
        return true;
    }
    if (name == QLatin1String("sub-visibility") && format == MPV_FORMAT_FLAG) {
        m_visible = *static_cast<int *>(data) != 0;
        emit visibleChanged();
        return true;
    }
    if (name == QLatin1String("sub-pos") && format == MPV_FORMAT_DOUBLE) {
        // Ignore the echo of our own applied value; a differing value means
        // the position was changed externally.
        const double raw = *static_cast<double *>(data);
        if (qAbs(raw - m_lastAppliedPos) > 0.001) {
            m_position = raw;
            emit positionChanged();
        }
        return true;
    }
    if (name == QLatin1String("sub-font-size") && format == MPV_FORMAT_DOUBLE) {
        m_fontSize = *static_cast<double *>(data);
        emit fontSizeChanged();
        return true;
    }
    if (name == QLatin1String("secondary-sid") && format == MPV_FORMAT_INT64) {
        m_secondaryTrack = *static_cast<int64_t *>(data);
        emit secondaryTrackChanged();
        return true;
    }
    return false;
}
