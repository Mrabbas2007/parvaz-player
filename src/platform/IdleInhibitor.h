#pragma once

#include <QObject>

class MpvObject;

namespace parvaz {

/// Inhibits desktop idle/blanking while mpv is actually playing, releases it
/// on pause/stop/exit.
/// On Linux: Talks to org.freedesktop.ScreenSaver via async D-Bus calls.
/// On Windows: Calls SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED).
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
#if !defined(_WIN32)
    static void sendUninhibit(quint32 cookie);
#endif

    MpvObject *m_mpv = nullptr;
    quint32 m_cookie = 0;
    bool m_wanted = false;
    bool m_acquirePending = false;
};

} // namespace parvaz
