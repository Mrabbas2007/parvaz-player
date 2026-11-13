#pragma once

#include <QObject>

class MpvObject;

namespace parvaz {

/// Inhibits desktop idle/blanking while mpv is actually playing, releases it
/// on pause/stop/exit. Talks to org.freedesktop.ScreenSaver via async calls
/// only -- never blocks the GUI thread. The desired state (m_wanted) is kept
/// separate from the held cookie (m_cookie) so a pause that arrives while the
/// Inhibit reply is still in flight is not lost.
class IdleInhibitor : public QObject {
    Q_OBJECT

public:
    explicit IdleInhibitor(MpvObject *mpv, QObject *parent = nullptr);
    ~IdleInhibitor() override;

private slots:
    void refresh();

private:
    void acquire();
    void release();
    static void sendUninhibit(quint32 cookie);

    MpvObject *m_mpv = nullptr;
    quint32 m_cookie = 0;
    bool m_wanted = false;
    bool m_acquirePending = false;
};

} // namespace parvaz
