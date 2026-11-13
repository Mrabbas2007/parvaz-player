#include <clocale>
#include <cmath>
#include <cstdio>
#include <functional>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QCommandLineParser>
#include <QFile>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>
#include <qqml.h>

#include "mpv/MpvObject.h"
#include "mpv/MpvFboItem.h"
#include "platform/IdleInhibitor.h"
#include "platform/MprisService.h"
#include "playback/PlaybackHistory.h"

/// Each step applies a change, then checks it on a later event-loop turn:
/// mpv reports the new value only through the observe callbacks, which need a
/// turn of the GUI event loop to arrive.
static int failures = 0;

// Positions recorded by the frame-step checks, read back on a later turn.
static double g_idlePos = 0.0;   // where we were before a deliberate no-op
static double g_stepFrom = 0.0;  // where we were before frameStep(1)
static double g_backFrom = 0.0;  // where we were before frameStep(-1)
static int g_trackCountBeforeAdd = 0;   // tracks before addSubtitleFile()

static void check(const char *what, bool ok)
{
    std::fprintf(stderr, "parvaz check: %s ... %s\n", what, ok ? "PASS" : "FAIL");
    if (!ok)
        ++failures;
}

/// One line per track, once, so a human can eyeball the node parse.
static void dumpTracks(const MpvObject &mpv)
{
    const QVariantList tracks = mpv.tracks();
    std::fprintf(stderr, "parvaz tracks: %d\n", int(tracks.size()));
    for (const QVariant &v : tracks) {
        const QVariantMap t = v.toMap();
        std::fprintf(stderr,
                     "parvaz track: id=%lld type=%s lang=%s title=%s external=%d selected=%d\n",
                     static_cast<long long>(t.value(QStringLiteral("id")).toLongLong()),
                     qPrintable(t.value(QStringLiteral("type")).toString()),
                     qPrintable(t.value(QStringLiteral("lang")).toString()),
                     qPrintable(t.value(QStringLiteral("title")).toString()),
                     t.value(QStringLiteral("external")).toBool() ? 1 : 0,
                     t.value(QStringLiteral("selected")).toBool() ? 1 : 0);
    }
}

static bool hasAudioTrack(const MpvObject &mpv)
{
    for (const QVariant &v : mpv.tracks()) {
        if (v.toMap().value(QStringLiteral("type")).toString() == QLatin1String("audio"))
            return true;
    }
    return false;
}

/// Counts tracks of one type that came from an external file.
static int externalSubCount(const MpvObject &mpv)
{
    int n = 0;
    for (const QVariant &v : mpv.tracks()) {
        const QVariantMap t = v.toMap();
        if (t.value(QStringLiteral("type")).toString() == QLatin1String("sub") &&
            t.value(QStringLiteral("external")).toBool())
            ++n;
    }
    return n;
}

/// The external sub track that is currently selected, or -1.
static qint64 selectedSubId(const MpvObject &mpv)
{
    for (const QVariant &v : mpv.tracks()) {
        const QVariantMap t = v.toMap();
        if (t.value(QStringLiteral("type")).toString() == QLatin1String("sub") &&
            t.value(QStringLiteral("external")).toBool() &&
            t.value(QStringLiteral("selected")).toBool())
            return t.value(QStringLiteral("id")).toLongLong();
    }
    return -1;
}

/// Title of the external sub track that is currently selected, or "".
static QString selectedSubTitle(const MpvObject &mpv)
{
    for (const QVariant &v : mpv.tracks()) {
        const QVariantMap t = v.toMap();
        if (t.value(QStringLiteral("type")).toString() == QLatin1String("sub") &&
            t.value(QStringLiteral("external")).toBool() &&
            t.value(QStringLiteral("selected")).toBool())
            return t.value(QStringLiteral("title")).toString();
    }
    return QString();
}

/// True when the string holds at least one Arabic-block codepoint. Proves the
/// bytes survived decode: a mojibake or empty result has none of these.
static bool hasArabicScript(const QString &s)
{
    for (const QChar c : s) {
        const ushort u = c.unicode();
        if (u >= 0x0600 && u <= 0x06FF)
            return true;
    }
    return false;
}

/// Starts the smoke sequence; returns immediately. The sequence runs from
/// queued timers and calls exit(0) / exit(1) when it is done.
static void runSmokeChecks(QGuiApplication *app, MpvObject &mpv)
{
    // Kept alive by the queued timers below; a local std::function would dangle.
    auto next = std::make_shared<std::function<void(int)>>();
    *next = [&mpv, app, next](int step) {
        switch (step) {
        case 0:  check("frames >= 30", parvazRenderedFrames() >= 30);
                 check("position advanced", mpv.position() > 0.0);
                 break;
        case 1:  check("duration known", mpv.duration() > 0.0);
                 mpv.setPaused(true);
                 break;
        case 2:  check("paused after setPaused(true)", mpv.paused());
                 mpv.setVolume(50);
                 break;
        case 3:  check("volume == 50", mpv.volume() == 50);
                 mpv.setMuted(true);
                 break;
        case 4:  check("muted == true", mpv.muted());
                 mpv.setMuted(false);
                 break;
        case 5:  check("muted == false", !mpv.muted());
                 mpv.setSpeed(2.0);
                 break;
        case 6:  check("speed == 2.0", qAbs(mpv.speed() - 2.0) < 0.001);
                 mpv.setSpeed(1.0);
                 break;
        case 7:  check("speed == 1.0", qAbs(mpv.speed() - 1.0) < 0.001);
                 mpv.seekAbsolute(2.0);
                 break;
        case 8:  check("seekAbsolute(2.0) within 0.3s", qAbs(mpv.position() - 2.0) <= 0.3);
                 mpv.seekAbsolute(1.0);
                 break;
        case 9:  check("seekAbsolute(1.0) within 0.3s", qAbs(mpv.position() - 1.0) <= 0.3);
                 // Baseline: paused, and nothing asks mpv to step.
                 g_idlePos = mpv.position();
                 break;
        case 10: // Still paused with no step requested: time-pos must not move.
                 check("no step: position stayed put",
                       qAbs(mpv.position() - g_idlePos) < 0.001);
                 g_stepFrom = mpv.position();
                 mpv.frameStep(1);
                 break;
        case 11: {
            const double fwd = mpv.position() - g_stepFrom;
            // One frame of the 30 fps test clip is 0.0333s; allow slack both
            // ways so a no-op and a multi-second jump both fail.
            check("frameStep(1) advanced ~1 frame", fwd >= 0.01 && fwd <= 0.1);
            check("frameStep(1) left us paused", mpv.paused());
            g_backFrom = mpv.position();
            mpv.frameStep(-1);
            break;
        }
        case 12: {
            // Stepping back must not run the clock forward.
            const double back = mpv.position() - g_backFrom;
            check("frameStep(-1) did not advance", back <= 0.001);
            check("frameStep(-1) left us paused", mpv.paused());
            break;
        }
        case 13: {
            dumpTracks(mpv);
            check("tracks is not empty", !mpv.tracks().isEmpty());
            check("tracks has an audio track", hasAudioTrack(mpv));
            check("tracks has an external sub track (sidecar)", externalSubCount(mpv) >= 1);
            break;
        }
        case 14:
            // Subtitles only decode while the clock runs, so unpause first.
            mpv.setPaused(false);
            mpv.seekAbsolute(0.2);
            break;
        case 15: {
            const QString text = mpv.subtitleText();
            std::fprintf(stderr, "parvaz check: sub-text at %.2fs = [%s]\n",
                         mpv.position(), qPrintable(text));
            check("subtitleText() non-empty after seek into cue 1", !text.isEmpty());
            check("subtitleText() holds Arabic script", hasArabicScript(text));
            break;
        }
        case 16:
            g_trackCountBeforeAdd = int(mpv.tracks().size());
            mpv.addSubtitleFile(QStringLiteral(PARVAZ_TESTS_DIR "/fa-cp1256.srt"));
            break;
        case 17: {
            const qint64 newId = selectedSubId(mpv);
            check("sub-add grew the track list by one",
                  int(mpv.tracks().size()) == g_trackCountBeforeAdd + 1);
            check("sub-add selected an external sub track", newId >= 0);
            // Without this the cp1256 assertion below could pass on the UTF-8
            // sidecar still being the selected track.
            std::fprintf(stderr, "parvaz check: selected sub title = [%s]\n",
                         qPrintable(selectedSubTitle(mpv)));
            check("sub-add selected the cp1256 file",
                  selectedSubTitle(mpv).contains(QStringLiteral("cp1256")));
            break;
        }
        case 18:
            mpv.setSubtitleCodepage(QStringLiteral("cp1256"));
            break;
        case 19:
            check("sub-codepage reads back cp1256",
                  mpv.subtitleCodepage() == QLatin1String("cp1256"));
            // Re-seek so a cue is on screen for the assertion below.
            mpv.seekAbsolute(0.2);
            break;
        case 20: {
            const QString text = mpv.subtitleText();
            std::fprintf(stderr, "parvaz check: sub-text after cp1256 reload = [%s]\n",
                         qPrintable(text));
            check("cp1256 subtitle decodes to text", !text.isEmpty());
            check("cp1256 subtitle holds Arabic script", hasArabicScript(text));
            break;
        }
        case 21:
            mpv.disableSubtitles();
            break;
        case 22:
            check("disableSubtitles() empties sub-text", mpv.subtitleText().isEmpty());
            break;
        case 23:
            mpv.setSubtitleDelay(0.5);
            break;
        case 24:
            check("subtitleDelay round-trips 0.5", qAbs(mpv.subtitleDelay() - 0.5) < 0.001);
            mpv.setSubtitleDelay(0.0);
            mpv.setSubtitleFontSize(50.0);
            mpv.setSubtitlePosition(120.0);
            break;
        case 25:
            // Both of these are mpv Float options; a wrong format would show up
            // as the value never moving.
            check("subtitleFontSize round-trips 50",
                  qAbs(mpv.subtitleFontSize() - 50.0) < 0.001);
            check("subtitlePosition round-trips 120",
                  qAbs(mpv.subtitlePosition() - 120.0) < 0.001);
            mpv.setSubtitleFontSize(38.0);
            mpv.setSubtitlePosition(100.0);
            // Engage bottom bar offset to verify subtitles shift up when the bar shows.
            mpv.setBottomBarOffset(true, 76.0, 720.0);
            break;
        case 26: {
            double rawSubPos = 0.0;
            mpv_get_property(mpv.handle(), "sub-pos", MPV_FORMAT_DOUBLE, &rawSubPos);
            check("bottom bar visible lifts subtitle position (< 90)", rawSubPos < 90.0 && rawSubPos > 0.0);
            mpv.setBottomBarOffset(false, 76.0, 720.0);
            break;
        }
        case 27: {
            double rawSubPos = 0.0;
            mpv_get_property(mpv.handle(), "sub-pos", MPV_FORMAT_DOUBLE, &rawSubPos);
            check("bottom bar hidden restores subtitle position to 100", qAbs(rawSubPos - 100.0) < 0.001);
            mpv.adjustSpeed(0.1);
            break;
        }
        case 28:
            check("adjustSpeed(+0.1) moved speed to 1.1", qAbs(mpv.speed() - 1.1) < 0.001);
            mpv.resetSpeed();
            break;
        case 29: {
            check("resetSpeed() restored speed to 1.0", qAbs(mpv.speed() - 1.0) < 0.001);
            const QString testDb = QStringLiteral("/tmp/test_parvaz_history.db");
            QFile::remove(testDb);
            {
                parvaz::PlaybackHistory history(&mpv, testDb);
                check("resumePosition initially 0", history.resumePosition(QStringLiteral("/dummy/video.mkv")) == 0.0);
            }
            QFile::remove(testDb);
            break;
        }
        default: {
            const QString hw = mpv.hwdec();
            std::fprintf(stderr,
                         "parvaz smoke: frames=%d position=%.2fs duration=%.2fs hwdec=%s %s\n",
                         parvazRenderedFrames(), mpv.position(), mpv.duration(),
                         hw.isEmpty() ? "none" : qPrintable(hw),
                         failures == 0 ? "OK" : "FAILED");
            QCoreApplication::exit(failures == 0 ? 0 : 1);
            return;
        }
        }
        // Action steps that need mpv to actually act (seek, reload, option change)
        // get a longer beat before the assertion on the next turn.
        const int delay = (step == 14 || step == 18 || step == 19 || step == 21 || step == 23
                               || step == 24 || step == 25 || step == 26 || step == 27 || step == 28)
                              ? 500 : 250;
        QTimer::singleShot(delay, app, [next, step]() { (*next)(step + 1); });
    };

    QTimer::singleShot(2500, app, [next]() { (*next)(0); });
}

int main(int argc, char *argv[])
{
    // libmpv's public render API speaks OpenGL (or software), and
    // QQuickFramebufferObject needs the OpenGL RHI backend. Must be set
    // before any QQuickWindow is created.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Parvaz"));
    app.setOrganizationName(QStringLiteral("Parvaz"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Parvaz - a PotPlayer-style player for Linux"));
    parser.addHelpOption();
    const QCommandLineOption smokeOpt(QStringLiteral("smoke"),
                                      QStringLiteral("Load the file, run ~2.5s, exit 0 if it really played (CI)"));
    parser.addOption(smokeOpt);
    const QCommandLineOption benchOpt(QStringLiteral("bench"),
                                      QStringLiteral("Run <seconds> and print a performance line"), QStringLiteral("seconds"));
    parser.addOption(benchOpt);
    parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("Media file to open"));
    parser.process(app);

    qmlRegisterType<MpvFboItem>("Parvaz", 1, 0, "MpvFboItem");

    // libmpv refuses to run when LC_NUMERIC is not "C".
    // Must happen AFTER QGuiApplication, which resets the locale.
    std::setlocale(LC_NUMERIC, "C");

    // Declared before the engine so it outlives every QML object.
    MpvObject mpv;
    qmlRegisterSingletonInstance("Parvaz", 1, 0, "Mpv", &mpv);

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() {
                         std::fprintf(stderr, "parvaz: QML root object creation failed\n");
                         QCoreApplication::exit(1);
                     }, Qt::QueuedConnection);

    engine.load(QUrl(QStringLiteral(PARVAZ_QML_DIR "/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        std::fprintf(stderr, "parvaz: could not load QML\n");
        return 1;
    }

    // Desktop integration, normal mode only: --smoke/--bench stay hermetic
    // (no bus name to collide with a running instance, no idle inhibit,
    // no resume seeking) so `check` is reproducible. Both are parented to the
    // app, on the GUI thread; all their D-Bus calls are async.
    if (!parser.isSet(smokeOpt) && !parser.isSet(benchOpt)) {
        new parvaz::MprisService(&mpv, &app);
        new parvaz::IdleInhibitor(&mpv, &app);
        new parvaz::PlaybackHistory(&mpv, QString(), &app);
    }

    // Deferred internally until the render context owns the video output.
    const QStringList files = parser.positionalArguments();
    if (!files.isEmpty())
        mpv.loadFile(files.first());

    if (parser.isSet(smokeOpt)) {
        // runSmokeChecks() exits the process itself when the sequence is done.
        runSmokeChecks(&app, mpv);
    }

    if (parser.isSet(benchOpt)) {
        const int secs = qMax(1, parser.value(benchOpt).toInt());
        QTimer::singleShot(secs * 1000, &app, [&mpv, secs]() {
            double rssMb = 0.0;
            if (QFile f(QStringLiteral("/proc/self/statm")); f.open(QIODevice::ReadOnly)) {
                const QList<QByteArray> parts = f.readAll().split(' ');
                if (parts.size() > 1)
                    rssMb = parts.at(1).toDouble() * 4096.0 / (1024.0 * 1024.0);
            }
            std::fprintf(stderr,
                         "parvaz bench: %ds frames=%d pos=%.2fs rss=%.0fMB %s\n",
                         secs, parvazRenderedFrames(), mpv.position(), rssMb,
                         qPrintable(mpv.statsLine()));
            QCoreApplication::exit(0);
        });
    }

    return app.exec();
}
