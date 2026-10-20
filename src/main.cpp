// oligarchy-tanks — retro 2-player local tank battle vs AI.

#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QLoggingCategory>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>

#include "backend.h"
#include "gamepad.h"
#include "theme.h"

// Passed by the .pro (DEFINES += OLIGARCHY_TANKS_VERSION="0.2.0"), but a
// direct compile still needs a version to name.
#ifndef OLIGARCHY_TANKS_VERSION
#define OLIGARCHY_TANKS_VERSION "0.2.0"
#endif

int main(int argc, char *argv[]) {
    // The FFmpeg version note from QtMultimedia is benign startup info.
    QLoggingCategory::setFilterRules(QStringLiteral("qt.multimedia.ffmpeg.info=false"));
    // Never serve stale compiled QML from the disk cache during development.
    qputenv("QML_DISABLE_DISK_CACHE", QByteArrayLiteral("1"));

    QGuiApplication app(argc, argv);
    app.setApplicationName("oligarchy-tanks");
    app.setApplicationVersion("0.1.0");

    // Matches the window to tanks.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed icon.
    app.setDesktopFileName("oligarchy-tanks");
    app.setWindowIcon(QIcon::fromTheme("oligarchy-tanks"));

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
    // Pads are read straight from /dev/input/event* (Linux only: evdev). A pad
    // joins a player by pressing START; with none connected nothing changes.
    // The reader writes to stderr when OLIGARCHY_TANKS_PAD_DEBUG is set.
    // Other platforms build without it and play on the keyboard.
#if defined(Q_OS_LINUX) && !defined(Q_OS_WASM)
    Gamepad gamepad(&backend);
    gamepad.start();
#endif

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
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
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}
