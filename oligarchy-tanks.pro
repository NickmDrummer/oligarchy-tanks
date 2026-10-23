QT += core gui qml quick quickcontrols2 multimedia

CONFIG += c++17 release
TARGET = oligarchy-tanks
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/padmap.h \
    src/soundboard.h \
    src/theme.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/padmap.cpp \
    src/soundboard.cpp \
    src/theme.cpp

# Gamepads: SDL2 is the reader for macOS and Windows, and it is used on Linux
# too when it is installed (one code path for the three). The evdev reader
# stays as the Linux fallback: without SDL the build still has pads.
#
#   qmake CONFIG+=sdlpad   force it with -lSDL2 (the Qt Installer route on
#                          Windows, where there is no pkg-config)
#   qmake CONFIG+=nosdl    keep evdev on Linux
CONFIG += link_pkgconfig
!contains(CONFIG, nosdl) {
    packagesExist(sdl2) {
        DEFINES += OLIGARCHY_TANKS_SDL
        PKGCONFIG += sdl2
        SOURCES += src/padsdl.cpp
        HEADERS += src/padsdl.h
    } else:contains(CONFIG, sdlpad) {
        DEFINES += OLIGARCHY_TANKS_SDL
        LIBS += -lSDL2
        SOURCES += src/padsdl.cpp
        HEADERS += src/padsdl.h
    }
}

# The evdev reader and the rumble are Linux-only (main.cpp guards the use).
linux {
    SOURCES += src/gamepad.cpp src/rumble.cpp
    HEADERS += src/gamepad.h src/rumble.h
}

RESOURCES += src/resources.qrc

# The release version the footer and the manual name (round 21.10).
DEFINES += OLIGARCHY_TANKS_VERSION=\\\"0.2.3\\\"
VERSION = 0.2.3

# For a release build, uncomment the next line (or pass DEFINES+=OLIGARCHY_TANKS_RELEASE
# to qmake). The footer then shows the version alone, without the build stamp.
# DEFINES += OLIGARCHY_TANKS_RELEASE
