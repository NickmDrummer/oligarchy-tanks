QT += core gui qml quick quickcontrols2 multimedia dbus

CONFIG += c++17 release
TARGET = tanks
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/theme.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/theme.cpp

RESOURCES += src/resources.qrc
