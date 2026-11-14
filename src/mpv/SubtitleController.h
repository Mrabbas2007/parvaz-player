#pragma once

#include <QObject>
#include <QString>

#include <mpv/client.h>

/// Subtitle playback state and mpv wiring, extracted from MpvObject.
///
/// Owns the sub-* state (delay, visibility, position, font size), the
/// bottom-bar offset compensation, the legacy codepage handling and the
/// secondary subtitle track. GUI-thread only, like all mpv_handle access.
/// MpvObject keeps the QML-facing Q_PROPERTYs and delegates here; property
/// change events are forwarded from MpvObject::drainEvents via
/// onMpvProperty().
class SubtitleController : public QObject {
    Q_OBJECT
public:
    explicit SubtitleController(QObject *parent = nullptr);

    /// mpv handle becomes available inside MpvObject's constructor body;
    /// call once right after mpv_create() succeeds.
    void setHandle(mpv_handle *mpv) { m_mpv = mpv; }

    double delay() const { return m_delay; }
    void setDelay(double seconds);

    bool visible() const { return m_visible; }
    void setVisible(bool visible);

    /// User-chosen position, percent from the top, mpv range 0..150.
    /// The applied sub-pos additionally carries the bottom-bar shift; the
    /// echo suppression in onMpvProperty keeps the user value intact.
    double position() const { return m_position; }
    void setPosition(double percentFromTop);

    double fontSize() const { return m_fontSize; }
    void setFontSize(double size);

    /// Current secondary subtitle track id (mpv "secondary-sid"), -1 when none.
    qint64 secondaryTrack() const { return m_secondaryTrack; }

    /// Updates the bottom control bar offset so subtitles shift above the
    /// bar whenever the bar is visible on screen.
    void setBottomBarOffset(bool visible, double barHeight, double windowHeight);

    /// Loads an external subtitle file and selects it ("select" flag).
    void addSubtitleFile(const QString &path);

    /// Forces a legacy encoding ("sub-codepage"), then reloads the current
    /// subtitle so it is re-decoded.
    void setCodepage(const QString &codepage);
    QString codepage() const;
    void reload();

    /// Text of the subtitle on screen right now, empty when nothing shows.
    QString text() const;

    void deselect();            // "sid" = "no", track stays loaded
    void cycleTrack();          // cycle sid
    void cycleSecondaryTrack(); // cycle secondary-sid
    void selectSecondaryTrack(qint64 id);

    /// Handles one property-change event forwarded by drainEvents.
    /// Returns true when the event belonged to this controller (it has then
    /// already emitted its change signal).
    bool onMpvProperty(const QString &name, mpv_format format, void *data);

signals:
    void delayChanged();
    void visibleChanged();
    void positionChanged();
    void fontSizeChanged();
    void secondaryTrackChanged();

private:
    void applyPosition();

    mpv_handle *m_mpv = nullptr;

    double m_delay = 0.0;
    bool m_visible = true;
    double m_position = 100.0;
    double m_fontSize = 38.0;
    qint64 m_secondaryTrack = -1;

    bool m_bottomBarVisible = false;
    double m_bottomBarHeight = 0.0;
    double m_windowHeight = 0.0;
    double m_lastAppliedPos = 100.0;
};
