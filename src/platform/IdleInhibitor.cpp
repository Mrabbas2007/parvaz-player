#include "platform/IdleInhibitor.h"

#include <QCoreApplication>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#endif

#include "mpv/MpvObject.h"

#include <cstdio>

namespace parvaz {

IdleInhibitor::IdleInhibitor(MpvObject *mpv, QObject *parent)
    : QObject(parent), m_mpv(mpv)
{
    Q_ASSERT(m_mpv);
    // pausedChanged doubles as the pause/stop signal; durationChanged doubles
    // as "a file is (no longer) open" (keep-open pauses at end-of-file).
    connect(m_mpv, &MpvObject::pausedChanged, this, &IdleInhibitor::refresh);
    connect(m_mpv, &MpvObject::durationChanged, this, &IdleInhibitor::refresh);
    refresh();
}

IdleInhibitor::~IdleInhibitor()
{
#if defined(_WIN32)
    if (m_wanted)
        SetThreadExecutionState(ES_CONTINUOUS);
#else
    // Fire-and-forget: send() writes to the bus socket and returns; a blocking
    // UnInhibit here would hang shutdown on a busy daemon.
    if (m_cookie != 0)
        sendUninhibit(m_cookie);
#endif
}

void IdleInhibitor::refresh()
{
    // Playing == not paused and a file with a duration is open.
    m_wanted = !m_mpv->paused() && m_mpv->duration() > 0.0;
#if defined(_WIN32)
    if (m_wanted)
        acquire();
    else
        release();
#else
    if (m_wanted && m_cookie == 0 && !m_acquirePending)
        acquire();
    else if (!m_wanted && m_cookie != 0)
        release();
#endif
}

void IdleInhibitor::acquire()
{
#if defined(_WIN32)
    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
    std::fprintf(stderr, "parvaz idle: windows sleep inhibited\n");
#else
    m_acquirePending = true;
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.ScreenSaver"),
        QStringLiteral("/org/freedesktop/ScreenSaver"),
        QStringLiteral("org.freedesktop.ScreenSaver"),
        QStringLiteral("Inhibit"));
    msg.setArguments({QCoreApplication::applicationName(),
                      QStringLiteral("playing video")});

    auto *watcher = new QDBusPendingCallWatcher(
        QDBusConnection::sessionBus().asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *w) {
                m_acquirePending = false;
                QDBusPendingReply<quint32> reply = *w;
                if (reply.isError()) {
                    std::fprintf(stderr, "parvaz idle: inhibit failed: %s\n",
                                 qPrintable(reply.error().message()));
                } else {
                    m_cookie = reply.value();
                    std::fprintf(stderr, "parvaz idle: inhibit cookie %u\n", m_cookie);
                    // Paused while the request was in flight: undo right away.
                    if (!m_wanted)
                        release();
                }
                w->deleteLater();
            });
#endif
}

void IdleInhibitor::release()
{
#if defined(_WIN32)
    SetThreadExecutionState(ES_CONTINUOUS);
    std::fprintf(stderr, "parvaz idle: windows sleep uninhibited\n");
#else
    if (m_cookie == 0)
        return;
    sendUninhibit(m_cookie);
    std::fprintf(stderr, "parvaz idle: uninhibit cookie %u\n", m_cookie);
    m_cookie = 0;
#endif
}

#if !defined(_WIN32)
void IdleInhibitor::sendUninhibit(quint32 cookie)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.ScreenSaver"),
        QStringLiteral("/org/freedesktop/ScreenSaver"),
        QStringLiteral("org.freedesktop.ScreenSaver"),
        QStringLiteral("UnInhibit"));
    msg.setArguments({cookie});
    QDBusConnection::sessionBus().send(msg); // async, never waits for a reply
}
#endif

} // namespace parvaz
