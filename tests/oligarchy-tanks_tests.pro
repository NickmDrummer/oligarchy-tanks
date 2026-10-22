QT += core gui quick quickcontrols2 testlib multimedia
CONFIG += c++17 testcase
TARGET = oligarchy-tanks_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/backend.h ../src/padmap.h ../src/soundboard.h ../src/theme.h ../src/rumble.h
SOURCES += oligarchy-tanks_tests.cpp ../src/backend.cpp ../src/padmap.cpp ../src/soundboard.cpp ../src/theme.cpp ../src/rumble.cpp
