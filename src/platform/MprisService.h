#pragma once

#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class MpvObject;

namespace parvaz {

/// MPRIS facade: owns the bus name org.mpris.MediaPlayer2.parvaz and forwards
/// MpvObject's signals into org.freedesktop.DBus.Properties.PropertiesChanged.
/// No playback state lives here -- every property is read live from MpvObject
/// (GUI thread), so there is nothing to desynchronise. The two
/// QDBusAbstractAdaptors (root + Player) are declared in the .cpp; QtDBus
/// answers org.freedesktop.DBus.Properties Get/GetAll/Set for their Q_PROPERTYs
/// automatically. PropertiesChanged is emitted manually because QtDBus does
/// not do that for adaptors.
class MprisService : public QObject {
    Q_OBJECT

public:
    explicit MprisService(MpvObject *mpv, QObject *parent = nullptr);
    ~MprisService() override;

    bool isRegistered() const { return m_registered; }

    // ---- org.mpris.MediaPlayer2 (root) ----
    bool CanQuit() const { return true; }
    bool CanRaise() const { return true; }
    bool HasTrackList() const { return false; }
    QString Identity() const;
    QString DesktopEntry() const;
    QStringList SupportedUriSchemes() const;
    QStringList SupportedMimeTypes() const;
    void Raise();
    void Quit();

    // ---- org.mpris.MediaPlayer2.Player: methods ----
    void Next();
    void Previous();
    void Pause();
    void PlayPause();
    void Stop();
    void Play();
    void Seek(qlonglong offsetMicroseconds);
    void SetPosition(const QDBusObjectPath &trackId, qlonglong positionMicroseconds);
    void OpenUri(const QString &uri);

    // ---- org.mpris.MediaPlayer2.Player: properties ----
    QString PlaybackStatus() const;
    QVariantMap Metadata() const;
    double Volume() const;
    void SetVolume(double volume);
    double Rate() const;
    void SetRate(double rate);
    qlonglong Position() const;
    double MinimumRate() const { return 0.01; }
    double MaximumRate() const { return 100.0; }
    bool CanPlay() const { return true; }
    bool CanPause() const { return true; }
    bool CanSeek() const { return true; }
    bool CanControl() const { return true; }
    bool CanGoNext() const { return false; }
    bool CanGoPrevious() const { return false; }

private:
    void notifyPropertiesChanged(const QStringList &names);
    QVariant propertyValue(const QString &name) const;

    MpvObject *m_mpv = nullptr;
    QDBusConnection m_bus;
    bool m_registered = false;
    quint64 m_trackCounter = 0;
    QStringList m_mimeTypes;
};

} // namespace parvaz
