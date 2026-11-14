#include "platform/MprisService.h"

#include <QDBusAbstractAdaptor>
#include <QDBusError>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QUrl>
#include <QWindow>

#include "mpv/MpvObject.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace parvaz {

// ---- MPRIS root interface (org.mpris.MediaPlayer2) -------------------------

class MprisRootAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")
    Q_PROPERTY(bool CanQuit READ CanQuit CONSTANT)
    Q_PROPERTY(bool CanRaise READ CanRaise CONSTANT)
    Q_PROPERTY(bool HasTrackList READ HasTrackList CONSTANT)
    Q_PROPERTY(QString Identity READ Identity CONSTANT)
    Q_PROPERTY(QString DesktopEntry READ DesktopEntry CONSTANT)
    Q_PROPERTY(QStringList SupportedUriSchemes READ SupportedUriSchemes CONSTANT)
    Q_PROPERTY(QStringList SupportedMimeTypes READ SupportedMimeTypes CONSTANT)

public:
    explicit MprisRootAdaptor(MprisService *service)
        : QDBusAbstractAdaptor(service), m_service(service) {}

    bool CanQuit() const { return m_service->CanQuit(); }
    bool CanRaise() const { return m_service->CanRaise(); }
    bool HasTrackList() const { return m_service->HasTrackList(); }
    QString Identity() const { return m_service->Identity(); }
    QString DesktopEntry() const { return m_service->DesktopEntry(); }
    QStringList SupportedUriSchemes() const { return m_service->SupportedUriSchemes(); }
    QStringList SupportedMimeTypes() const { return m_service->SupportedMimeTypes(); }

public slots:
    void Raise() { m_service->Raise(); }
    void Quit() { m_service->Quit(); }

private:
    MprisService *m_service;
};

// ---- MPRIS Player interface (org.mpris.MediaPlayer2.Player) ----------------

class MprisPlayerAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
    Q_PROPERTY(QString PlaybackStatus READ PlaybackStatus CONSTANT)
    Q_PROPERTY(double Rate READ Rate WRITE SetRate)
    Q_PROPERTY(QVariantMap Metadata READ Metadata CONSTANT)
    Q_PROPERTY(double Volume READ Volume WRITE SetVolume)
    Q_PROPERTY(qlonglong Position READ Position CONSTANT)
    Q_PROPERTY(double MinimumRate READ MinimumRate CONSTANT)
    Q_PROPERTY(double MaximumRate READ MaximumRate CONSTANT)
    Q_PROPERTY(bool CanGoNext READ CanGoNext CONSTANT)
    Q_PROPERTY(bool CanGoPrevious READ CanGoPrevious CONSTANT)
    Q_PROPERTY(bool CanPlay READ CanPlay CONSTANT)
    Q_PROPERTY(bool CanPause READ CanPause CONSTANT)
    Q_PROPERTY(bool CanSeek READ CanSeek CONSTANT)
    Q_PROPERTY(bool CanControl READ CanControl CONSTANT)

public:
    explicit MprisPlayerAdaptor(MprisService *service)
        : QDBusAbstractAdaptor(service), m_service(service) {}

    QString PlaybackStatus() const { return m_service->PlaybackStatus(); }
    double Rate() const { return m_service->Rate(); }
    void SetRate(double rate) { m_service->SetRate(rate); }
    QVariantMap Metadata() const { return m_service->Metadata(); }
    double Volume() const { return m_service->Volume(); }
    void SetVolume(double volume) { m_service->SetVolume(volume); }
    qlonglong Position() const { return m_service->Position(); }
    double MinimumRate() const { return m_service->MinimumRate(); }
    double MaximumRate() const { return m_service->MaximumRate(); }
    bool CanGoNext() const { return m_service->CanGoNext(); }
    bool CanGoPrevious() const { return m_service->CanGoPrevious(); }
    bool CanPlay() const { return m_service->CanPlay(); }
    bool CanPause() const { return m_service->CanPause(); }
    bool CanSeek() const { return m_service->CanSeek(); }
    bool CanControl() const { return m_service->CanControl(); }

public slots:
    void Next() { m_service->Next(); }
    void Previous() { m_service->Previous(); }
    void Pause() { m_service->Pause(); }
    void PlayPause() { m_service->PlayPause(); }
    void Stop() { m_service->Stop(); }
    void Play() { m_service->Play(); }
    void Seek(qlonglong offset) { m_service->Seek(offset); }
    void SetPosition(const QDBusObjectPath &trackId, qlonglong pos)
    {
        m_service->SetPosition(trackId, pos);
    }
    void OpenUri(const QString &uri) { m_service->OpenUri(uri); }

private:
    MprisService *m_service;
};

// ---- MprisService -----------------------------------------------------------

MprisService::MprisService(MpvObject *mpv, QObject *parent)
    : QObject(parent), m_mpv(mpv), m_bus(QDBusConnection::sessionBus())
{
    Q_ASSERT(m_mpv);

    m_mimeTypes = QStringList{
        QStringLiteral("video/x-matroska"),   QStringLiteral("video/mp4"),
        QStringLiteral("video/webm"),         QStringLiteral("video/x-msvideo"),
        QStringLiteral("video/quicktime"),    QStringLiteral("video/mpeg"),
        QStringLiteral("video/x-flv"),        QStringLiteral("audio/mpeg"),
        QStringLiteral("audio/x-flac"),       QStringLiteral("audio/ogg"),
        QStringLiteral("audio/x-wav"),        QStringLiteral("application/ogg"),
    };

    // A fresh trackid for every loaded file (MPRIS: unique per track).
    connect(m_mpv, &MpvObject::fileLoaded, this, [this] { ++m_trackCounter; });

    // Parented to `this` (QDBusAbstractAdaptor takes the owner as parent).
    new MprisRootAdaptor(this);
    new MprisPlayerAdaptor(this);

    // Export the object first, own the name last: anything that starts
    // watching the name must find the object fully populated.
    if (!m_bus.registerObject(QStringLiteral("/org/mpris/MediaPlayer2"), this,
                              QDBusConnection::ExportAdaptors)) {
        std::fprintf(stderr, "parvaz mpris: registerObject failed: %s\n",
                     qPrintable(m_bus.lastError().message()));
        return;
    }
    if (!m_bus.registerService(QStringLiteral("org.mpris.MediaPlayer2.parvaz"))) {
        // Another instance owns the name (or the bus refused). One line, keep
        // going: the player is still fully usable without MPRIS.
        std::fprintf(stderr,
                     "parvaz mpris: could not own org.mpris.MediaPlayer2.parvaz (%s); "
                     "continuing without MPRIS\n",
                     qPrintable(m_bus.lastError().message()));
        return;
    }
    m_registered = true;

    // Everything below forwards MpvObject's own signals into PropertiesChanged.
    connect(m_mpv, &MpvObject::pausedChanged, this, [this] {
        notifyPropertiesChanged({QStringLiteral("PlaybackStatus")});
    });
    connect(m_mpv, &MpvObject::volumeChanged, this, [this] {
        notifyPropertiesChanged({QStringLiteral("Volume")});
    });
    connect(m_mpv, &MpvObject::speedChanged, this, [this] {
        notifyPropertiesChanged({QStringLiteral("Rate")});
    });
    // Title, duration and trackid change together on file transitions, and the
    // status flips between Stopped and Playing with them.
    connect(m_mpv, &MpvObject::fileLoaded, this, [this] {
        notifyPropertiesChanged({QStringLiteral("Metadata"),
                                 QStringLiteral("PlaybackStatus")});
    });
    connect(m_mpv, &MpvObject::mediaTitleChanged, this, [this] {
        notifyPropertiesChanged({QStringLiteral("Metadata")});
    });
    connect(m_mpv, &MpvObject::durationChanged, this, [this] {
        notifyPropertiesChanged({QStringLiteral("Metadata"),
                                 QStringLiteral("PlaybackStatus")});
    });
    connect(m_mpv, &MpvObject::metadataChanged, this, [this] {
        notifyPropertiesChanged({QStringLiteral("Metadata")});
    });
    connect(m_mpv, &MpvObject::coverArtUrlChanged, this, [this] {
        notifyPropertiesChanged({QStringLiteral("Metadata")});
    });
    connect(m_mpv, &MpvObject::playlistChanged, this, [this] {
        notifyPropertiesChanged({QStringLiteral("CanGoNext"),
                                 QStringLiteral("CanGoPrevious")});
    });
}

MprisService::~MprisService()
{
    if (!m_registered)
        return;
    m_bus.unregisterService(QStringLiteral("org.mpris.MediaPlayer2.parvaz"));
    m_bus.unregisterObject(QStringLiteral("/org/mpris/MediaPlayer2"));
}

// ---- root interface ---------------------------------------------------------

QString MprisService::Identity() const
{
    return QStringLiteral("Parvaz Player");
}

QString MprisService::DesktopEntry() const
{
    return QStringLiteral("parvaz");
}

QStringList MprisService::SupportedUriSchemes() const
{
    return {QStringLiteral("file")};
}

QStringList MprisService::SupportedMimeTypes() const
{
    return m_mimeTypes;
}

void MprisService::Raise()
{
    // The player has exactly one top-level window; no pointer is plumbed in.
    const QWindowList windows = QGuiApplication::topLevelWindows();
    if (windows.isEmpty())
        return;
    QWindow *window = windows.first();
    if (window->windowStates() & Qt::WindowMinimized)
        window->showNormal();
    window->raise();
    window->requestActivate(); // Wayland: xdg-activation, best effort
}

void MprisService::Quit()
{
    QCoreApplication::quit();
}

// ---- Player interface: methods ----------------------------------------------

void MprisService::Next()
{
    if (m_mpv)
        m_mpv->next();
}

void MprisService::Previous()
{
    if (m_mpv)
        m_mpv->previous();
}

void MprisService::Pause() { m_mpv->setPaused(true); }
void MprisService::Play() { m_mpv->setPaused(false); }
void MprisService::PlayPause() { m_mpv->togglePause(); }

void MprisService::Stop()
{
    // No playlist yet: the closest honest stop is pause + rewind. keep-open
    // keeps the last frame; PlaybackStatus reports "Paused".
    m_mpv->seekAbsolute(0.0);
    m_mpv->setPaused(true);
}

void MprisService::Seek(qlonglong offsetMicroseconds)
{
    // MPRIS: offset relative to the current position, in microseconds.
    m_mpv->seekRelative(static_cast<double>(offsetMicroseconds) / 1e6);
}

void MprisService::SetPosition(const QDBusObjectPath &trackId,
                               qlonglong positionMicroseconds)
{
    Q_UNUSED(trackId); // single-track player for now
    m_mpv->seekAbsolute(static_cast<double>(positionMicroseconds) / 1e6);
}

void MprisService::OpenUri(const QString &uri)
{
    const QUrl url(uri);
    m_mpv->loadFile(url.isLocalFile() ? url.toLocalFile() : uri);
}

// ---- Player interface: properties -------------------------------------------

QString MprisService::PlaybackStatus() const
{
    // No duration means no file open (idle); that is MPRIS "Stopped".
    if (m_mpv->duration() <= 0.0)
        return QStringLiteral("Stopped");
    return m_mpv->paused() ? QStringLiteral("Paused") : QStringLiteral("Playing");
}

QVariantMap MprisService::Metadata() const
{
    QVariantMap m;
    // mpris:trackid must be a valid object path, not a plain string.
    const double duration = m_mpv->duration();
    if (duration <= 0.0) {
        m.insert(QStringLiteral("mpris:trackid"),
                 QVariant::fromValue(QDBusObjectPath(QStringLiteral("/"))));
        return m;
    }
    m.insert(QStringLiteral("mpris:trackid"),
             QVariant::fromValue(QDBusObjectPath(
                 QStringLiteral("/org/mpris/MediaPlayer2/Track/%1").arg(m_trackCounter))));
    // MPRIS length is microseconds; mpv duration is seconds.
    m.insert(QStringLiteral("mpris:length"),
             QVariant::fromValue(static_cast<qlonglong>(duration * 1e6)));
    const QString title = m_mpv->trackTitle();
    if (!title.isEmpty())
        m.insert(QStringLiteral("xesam:title"), title);
    const QString artist = m_mpv->trackArtist();
    if (!artist.isEmpty())
        m.insert(QStringLiteral("xesam:artist"), QStringList{artist});
    const QString album = m_mpv->trackAlbum();
    if (!album.isEmpty())
        m.insert(QStringLiteral("xesam:album"), album);
    const QString artUrl = m_mpv->coverArtUrl();
    if (!artUrl.isEmpty())
        m.insert(QStringLiteral("mpris:artUrl"), artUrl);
    return m;
}

double MprisService::Volume() const
{
    // MPRIS volume is a double in 0.0..1.0; mpv volume is 0..100.
    return std::clamp(m_mpv->volume() / 100.0, 0.0, 1.0);
}

void MprisService::SetVolume(double volume)
{
    m_mpv->setVolume(qRound(std::clamp(volume, 0.0, 1.0) * 100.0));
}

double MprisService::Rate() const
{
    return m_mpv->speed();
}

void MprisService::SetRate(double rate)
{
    // Clamped to mpv's 0.01..100 inside MpvObject::setSpeed.
    m_mpv->setSpeed(rate);
}

qlonglong MprisService::Position() const
{
    // MPRIS position is microseconds; mpv time-pos is seconds (and can be
    // briefly negative before the first frame lands).
    return static_cast<qlonglong>(std::max(0.0, m_mpv->position()) * 1e6);
}

bool MprisService::CanGoNext() const
{
    return m_mpv && m_mpv->hasNext();
}

bool MprisService::CanGoPrevious() const
{
    return m_mpv && m_mpv->hasPrevious();
}

// ---- PropertiesChanged -------------------------------------------------------

QVariant MprisService::propertyValue(const QString &name) const
{
    if (name == QLatin1String("PlaybackStatus"))
        return PlaybackStatus();
    if (name == QLatin1String("Metadata"))
        return Metadata();
    if (name == QLatin1String("Volume"))
        return Volume();
    if (name == QLatin1String("Rate"))
        return Rate();
    if (name == QLatin1String("CanGoNext"))
        return CanGoNext();
    if (name == QLatin1String("CanGoPrevious"))
        return CanGoPrevious();
    return {};
}

void MprisService::notifyPropertiesChanged(const QStringList &names)
{
    if (!m_registered)
        return;
    QVariantMap changed;
    for (const QString &name : names) {
        const QVariant value = propertyValue(name);
        if (value.isValid())
            changed.insert(name, value);
    }
    if (changed.isEmpty())
        return;

    // Emitted manually: QtDBus does not relay adaptor property changes.
    QDBusMessage msg = QDBusMessage::createSignal(
        QStringLiteral("/org/mpris/MediaPlayer2"),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"));
    msg.setArguments({QStringLiteral("org.mpris.MediaPlayer2.Player"),
                      QVariant::fromValue(changed), QStringList()});
    m_bus.send(msg);
}

} // namespace parvaz

#include "MprisService.moc"
