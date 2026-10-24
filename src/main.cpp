// oligarchy-tanks — retro 2-player local tank battle vs AI.

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QLoggingCategory>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QSysInfo>
#include <QUrl>

#if defined(OLIGARCHY_TANKS_SDL)
// SDL's header renames main() to SDL_main on Windows, which is what the
// SDL2main object that MinGW links expects (otherwise: "undefined reference
// to SDL_main"). It does nothing on the other platforms.
#include <SDL.h>
#endif

#include "backend.h"
#if defined(OLIGARCHY_TANKS_SDL_PADS)
#include "padsdl.h"
#elif defined(Q_OS_LINUX) && !defined(Q_OS_WASM)
#include "gamepad.h"
#endif
#include "rumble.h"
#include "soundboard.h"
#include "theme.h"

namespace {

// A GUI binary has no console on Windows: when the interface fails to load the
// user sees nothing at all (0.2.4 shipped a Windows zip that died exactly that
// way). Everything Qt says — warnings, QML import errors, the platform plugin
// — now goes to a log next to the executable as well as to stderr.
QFile *g_log = nullptr;

void logMessage(QtMsgType type, const QMessageLogContext &, const QString &message) {
    const char *level = "info";
    switch (type) {
    case QtDebugMsg: level = "debug"; break;
    case QtInfoMsg: level = "info"; break;
    case QtWarningMsg: level = "warning"; break;
    case QtCriticalMsg: level = "critical"; break;
    case QtFatalMsg: level = "fatal"; break;
    }
    const QString line = QStringLiteral("%1 [%2] %3")
            .arg(QDateTime::currentDateTime().toString(Qt::ISODate), QLatin1String(level), message);
    if (g_log && g_log->isOpen()) {
        g_log->write(line.toUtf8());
        g_log->write("\n");
        g_log->flush();
    }
    fprintf(stderr, "%s\n", qPrintable(line));
}

void openLog() {
    const QStringList candidates = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/oligarchy-tanks.log"),
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                + QStringLiteral("/oligarchy-tanks.log"),
    };
    for (const QString &path : candidates) {
        QDir().mkpath(QFileInfo(path).absolutePath());
        auto *file = new QFile(path);
        if (file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            g_log = file;
            break;
        }
        delete file;
    }
    qInstallMessageHandler(logMessage);
}

} // namespace

// Passed by the .pro (DEFINES += OLIGARCHY_TANKS_VERSION="0.2.1"), but a
// direct compile still needs a version to name.
#ifndef OLIGARCHY_TANKS_VERSION
#define OLIGARCHY_TANKS_VERSION "0.2.2"
#endif

int main(int argc, char *argv[]) {
    // The FFmpeg version note from QtMultimedia is benign startup info.
    QLoggingCategory::setFilterRules(QStringLiteral("qt.multimedia.ffmpeg.info=false"));
    // Never serve stale compiled QML from the disk cache during development.
    qputenv("QML_DISABLE_DISK_CACHE", QByteArrayLiteral("1"));

    QGuiApplication app(argc, argv);
    app.setApplicationName("oligarchy-tanks");
    openLog();
    qInfo("Oligarchy Tanks %s starting (%s)",
          OLIGARCHY_TANKS_VERSION, qPrintable(QSysInfo::prettyProductName()));

    // Matches the window to tanks.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed icon.
    app.setDesktopFileName("oligarchy-tanks");
    // The theme icon when the package is installed, the embedded copy
    // otherwise, so the window always carries the game's icon.
    QIcon icon = QIcon::fromTheme("oligarchy-tanks");
    if (icon.isNull())
        icon = QIcon(QStringLiteral(":/icons/oligarchy-tanks.png"));
    app.setWindowIcon(icon);

    // The UI draws its own pixel furniture (see PixelButton and KeyCap), so the
    // Controls style only shapes the window: plain Basic, no Material.
    QQuickStyle::setStyle("Basic");

    // The game's own pixel font: Silkscreen (SIL OFL, see src/fonts/OFL.txt),
    // bundled in the resources so the UI looks the same on any machine. Every
    // Text inherits it; none of them names a family.
    const int fontId = QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/Silkscreen-Regular.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/Silkscreen-Bold.ttf"));
    // Departure Mono: a more legible pixel mono for body/descriptive text.
    // Silkscreen stays the global font (titles and key caps inherit it); body
    // text opts into this one explicitly (see the manual).
    QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/DepartureMono-Regular.otf"));
    // The Omarchy wordmark font (MIT, see fonts/OmarchyFont-LICENSE.txt),
    // available on demand; only the cabinet marquee opts into it.
    QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/OmarchyFont.ttf"));
    if (fontId >= 0) {
        const QStringList families = QFontDatabase::applicationFontFamilies(fontId);
        if (!families.isEmpty()) {
            QFont appFont = app.font();
            appFont.setFamily(families.first());
            app.setFont(appFont);
        }
    }

    Theme theme;
    Backend backend;
    // The sounds live in a thread of their own: when the audio device
    // dies, Qt's own handling can wedge the thread that owns the sound
    // (see soundboard.h). In the GUI thread that freeze is the game.
    SoundBoard sounds;
    // A pad joins a player by pressing START; with none connected nothing
    // changes. SDL2 is the reader whenever it is built in (macOS, Windows,
    // and Linux when it is installed); Linux without SDL falls back to evdev.
    // Both write to stderr when OLIGARCHY_TANKS_PAD_DEBUG is set.
#if defined(OLIGARCHY_TANKS_SDL_PADS)
    PadSdl pad(&backend);
    // The audio device events travel through the same pump.
    pad.onAudioDeviceChange = [&sounds] { sounds.reopenAudio(); };
    pad.start();
#elif defined(Q_OS_LINUX) && !defined(Q_OS_WASM)
    Gamepad gamepad(&backend);
    gamepad.start();
#endif

#if defined(Q_OS_LINUX) && !defined(Q_OS_WASM)
    // Rumble on DualSense pads (rumble.h). It listens to the same play
    // events the sounds do, so no Backend change was needed. Two switches:
    // M is sound only, V is rumble only (the user's felt preference), and
    // OLIGARCHY_TANKS_RUMBLE=0 backs both up.
    Rumble rumble;
    const auto hush = [&rumble, &backend] {
        rumble.setMuted(!backend.soundEnabled() || !backend.rumbleEnabled());
    };
    QObject::connect(&backend, &Backend::playSound, &rumble, &Rumble::play);
    QObject::connect(&backend, &Backend::soundChanged, &rumble, hush);
    QObject::connect(&backend, &Backend::rumbleChanged, &rumble, hush);
    rumble.start();
#endif

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.rootContext()->setContextProperty("sounds", &sounds);
    // The version names the release the user is playing (round 21.10): the
    // footer and the manual show it. A development build appends the binary's
    // mtime so a screenshot always names the exact build; a release build
    // (OLIGARCHY_TANKS_RELEASE defined) shows the version alone.
    QCoreApplication::setApplicationVersion(QStringLiteral(OLIGARCHY_TANKS_VERSION));
    QString build = QStringLiteral("V") + QCoreApplication::applicationVersion();
#ifndef OLIGARCHY_TANKS_RELEASE
    const QDateTime built = QFileInfo(QCoreApplication::applicationFilePath()).lastModified();
    build += QStringLiteral("  ") + built.toString(QStringLiteral("MMM d HH:mm:ss"));
#endif
    engine.rootContext()->setContextProperty("buildId", build);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty()) {
        qCritical("the interface (qrc:/Main.qml) could not be loaded; see the messages above");
        return 1;
    }

    // The mouse has no business in a run (the user's report): the
    // pointer floats in the middle of the screen and takes the eyes
    // with it. While the game is on the board, the window's cursor is
    // blanked from here — Main.qml's Items have no cursorShape in this
    // Qt (Quick 6.12 dropped it), and a full-window MouseArea would
    // eat the hover the menu buttons want. The arrow is back for every
    // other state: the menus and the pause overlay are mouse-territory.
    QQuickWindow *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (window) {
        const auto shape = [window, &backend] {
            window->setCursor(QCursor(backend.state() == QStringLiteral("playing")
                    ? Qt::BlankCursor : Qt::ArrowCursor));
        };
        shape();
        QObject::connect(&backend, &Backend::stateChanged, window, shape);
    }

    return app.exec();
}
