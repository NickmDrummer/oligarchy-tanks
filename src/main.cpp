// tanks — retro 2-player local tank battle vs AI.

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

int main(int argc, char *argv[]) {
    // The FFmpeg version note from QtMultimedia is benign startup info.
    QLoggingCategory::setFilterRules(QStringLiteral("qt.multimedia.ffmpeg.info=false"));
    // Never serve stale compiled QML from the disk cache during development.
    qputenv("QML_DISABLE_DISK_CACHE", QByteArrayLiteral("1"));

    QGuiApplication app(argc, argv);
    app.setApplicationName("tanks");
    app.setApplicationVersion("0.1.0");

    // Matches the window to tanks.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed icon.
    app.setDesktopFileName("tanks");
    app.setWindowIcon(QIcon::fromTheme("tanks"));

    // The UI draws its own pixel furniture (see PixelButton and KeyCap), so the
    // Controls style only shapes the window: plain Basic, no Material.
    QQuickStyle::setStyle("Basic");

    // The game's own pixel font: Silkscreen (SIL OFL, see src/fonts/OFL.txt),
    // bundled in the resources so the UI looks the same on any machine. Every
    // Text inherits it; none of them names a family.
    const int fontId = QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/Silkscreen-Regular.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/Silkscreen-Bold.ttf"));
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
    // Pads are read straight from /dev/input/event*. A pad joins a player by
    // pressing START; with none connected nothing changes. The reader writes to
    // stderr when TANKS_PAD_DEBUG is set.
    Gamepad gamepad(&backend);
    gamepad.start();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    // Stamp the running binary's own mtime, so the footer always names the
    // exact build on screen even when only QML was recompiled.
    const QDateTime built = QFileInfo(QCoreApplication::applicationFilePath()).lastModified();
    engine.rootContext()->setContextProperty(
        "buildId", built.toString(QStringLiteral("MMM d yyyy HH:mm:ss")));
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}
