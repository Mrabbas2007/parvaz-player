#include <clocale>
#include <cstdio>

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

    // Deferred internally until the render context owns the video output.
    const QStringList files = parser.positionalArguments();
    if (!files.isEmpty())
        mpv.loadFile(files.first());

    if (parser.isSet(smokeOpt)) {
        QTimer::singleShot(2500, &app, [&mpv]() {
            const int frames = parvazRenderedFrames();
            const double pos = mpv.position();
            const QString hw = mpv.hwdec();
            std::fprintf(stderr,
                         "parvaz smoke: frames=%d position=%.2fs duration=%.2fs hwdec=%s\n",
                         frames, pos, mpv.duration(),
                         hw.isEmpty() ? "none" : qPrintable(hw));
            const bool ok = frames >= 30 && pos > 0.0;
            QCoreApplication::exit(ok ? 0 : 1);
        });
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
