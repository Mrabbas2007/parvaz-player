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
#include <qqml.h>

#include "mpv/MpvObject.h"
#include "mpv/MpvFboItem.h"
#include "platform/IdleInhibitor.h"
#include "platform/MprisService.h"

/// Each step applies a change, then checks it on a later event-loop turn:
/// mpv reports the new value only through the observe callbacks, which need a
/// turn of the GUI event loop to arrive.
static int failures = 0;

// Positions recorded by the frame-step checks, read back on a later turn.
static double g_idlePos = 0.0;   // where we were before a deliberate no-op
static double g_stepFrom = 0.0;  // where we were before frameStep(1)
static double g_backFrom = 0.0;  // where we were before frameStep(-1)

static void check(const char *what, bool ok)
{
    std::fprintf(stderr, "parvaz check: %s ... %s\n", what, ok ? "PASS" : "FAIL");
    if (!ok)
        ++failures;
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
        QTimer::singleShot(250, app, [next, step]() { (*next)(step + 1); });
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
    // (no bus name to collide with a running instance, no idle inhibit) so
    // `check` is reproducible. Both are parented to the app, on the GUI
    // thread; all their D-Bus calls are async.
    if (!parser.isSet(smokeOpt) && !parser.isSet(benchOpt)) {
        new parvaz::MprisService(&mpv, &app);
        new parvaz::IdleInhibitor(&mpv, &app);
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
