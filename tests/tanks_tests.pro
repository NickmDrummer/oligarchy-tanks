QT += core gui quick quickcontrols2 testlib dbus
CONFIG += c++17 testcase
TARGET = tanks_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/backend.h ../src/padmap.h ../src/theme.h
SOURCES += tanks_tests.cpp ../src/backend.cpp ../src/padmap.cpp ../src/theme.cpp
