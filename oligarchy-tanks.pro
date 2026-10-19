QT += core gui qml quick quickcontrols2 multimedia

CONFIG += c++17 release
TARGET = oligarchy-tanks
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/padmap.h \
    src/theme.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/padmap.cpp \
    src/theme.cpp

# The pad reader is evdev/sysfs on Linux; other platforms compile without it
# and play on the keyboard (main.cpp guards the Gamepad use).
linux {
    SOURCES += src/gamepad.cpp
    HEADERS += src/gamepad.h
}

RESOURCES += src/resources.qrc

# For a release build, uncomment the next line (or pass DEFINES+=OLIGARCHY_TANKS_RELEASE
# to qmake). The footer then shows "V0.1" instead of the build stamp.
# DEFINES += OLIGARCHY_TANKS_RELEASE
