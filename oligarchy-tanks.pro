QT += core gui qml quick quickcontrols2 multimedia dbus

CONFIG += c++17 release
TARGET = oligarchy-tanks
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/padmap.h \
    src/gamepad.h \
    src/theme.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/padmap.cpp \
    src/gamepad.cpp \
    src/theme.cpp

RESOURCES += src/resources.qrc

# For a release build, uncomment the next line (or pass DEFINES+=OLIGARCHY_TANKS_RELEASE
# to qmake). The footer then shows "V0.1" instead of the build stamp.
# DEFINES += OLIGARCHY_TANKS_RELEASE
