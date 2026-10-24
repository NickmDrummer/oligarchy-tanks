QT += core gui quick quickcontrols2 testlib
CONFIG += c++17 testcase
TARGET = oligarchy-tanks_tests
TEMPLATE = app

INCLUDEPATH += ../src

# The sound board is SDL2 now (soundengine.cpp); the same detection the app
# uses, so both build with the same dependency.
CONFIG += link_pkgconfig
packagesExist(sdl2) {
    PKGCONFIG += sdl2
} else {
    LIBS += -lSDL2
}

# The sounds come from the binary's resources, as in the app: loading them in
# the tests exercises the real path (the old board could not tell).
RESOURCES += ../src/resources.qrc

HEADERS += ../src/backend.h ../src/padmap.h ../src/soundboard.h ../src/soundengine.h ../src/theme.h ../src/rumble.h
SOURCES += oligarchy-tanks_tests.cpp ../src/backend.cpp ../src/padmap.cpp ../src/soundboard.cpp ../src/soundengine.cpp ../src/theme.cpp ../src/rumble.cpp
