QT += core gui qml quick quickcontrols2

CONFIG += c++17 release
TARGET = oligarchy-tanks
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/padmap.h \
    src/soundboard.h \
    src/soundengine.h \
    src/theme.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/padmap.cpp \
    src/soundboard.cpp \
    src/soundengine.cpp \
    src/theme.cpp

# SDL2 is a hard dependency: it plays the sound (the engine replaced Qt
# Multimedia, whose device monitor does not survive a device change) and it
# reads the pads outside Linux.
#
#   qmake CONFIG+=sdlpad   force it with -lSDL2 (the Qt Installer route on
#                          Windows, where there is no pkg-config)
#   qmake CONFIG+=nosdl    Linux only: read the pads with evdev instead of SDL
CONFIG += link_pkgconfig
SDL_FOUND = false
packagesExist(sdl2) {
    PKGCONFIG += sdl2
    SDL_FOUND = true
} else:contains(CONFIG, sdlpad) {
    LIBS += -lSDL2
    SDL_FOUND = true
}
!equals(SDL_FOUND, true) {
    error("SDL2 is required: install libsdl2 (Arch), sdl2 (Homebrew) or SDL2 (MSYS2), or pass CONFIG+=sdlpad")
}
# main.cpp uses this to pull in SDL.h (which renames main for SDL2main on
# Windows) and to start the SDL2 pad reader.
DEFINES += OLIGARCHY_TANKS_SDL
!contains(CONFIG, nosdl) {
    # nosdl means "read the pads with evdev on Linux": SDL still plays the
    # sound, but PadSdl is not built.
    DEFINES += OLIGARCHY_TANKS_SDL_PADS
    SOURCES += src/padsdl.cpp
    HEADERS += src/padsdl.h
}

# The evdev reader and the rumble are Linux-only (main.cpp guards the use).
linux {
    SOURCES += src/gamepad.cpp src/rumble.cpp
    HEADERS += src/gamepad.h src/rumble.h
}

RESOURCES += src/resources.qrc

# The release version the footer and the manual name (round 21.10).
DEFINES += OLIGARCHY_TANKS_VERSION=\\\"0.2.4\\\"
VERSION = 0.2.4

# The application icons: the Windows executable and the macOS bundle carry
# the same drawing as the Linux one (tools/make-icon.py writes all three).
win32: RC_ICONS = pkgbuild/oligarchy-tanks.ico
macx: ICON = pkgbuild/oligarchy-tanks.icns
macx: QMAKE_BUNDLE = oligarchy-tanks
macx: QMAKE_TARGET_BUNDLE_PREFIX = com.nickmdrummer

# For a release build, uncomment the next line (or pass DEFINES+=OLIGARCHY_TANKS_RELEASE
# to qmake). The footer then shows the version alone, without the build stamp.
# DEFINES += OLIGARCHY_TANKS_RELEASE
