QT += core gui qml quick quickcontrols2 testlib
TEMPLATE = app
TARGET = asteria-settings-tests
CONFIG += console c++17 testcase
CONFIG -= app_bundle
DEFINES += PYROWAVE_EXPERIMENTAL=1
INCLUDEPATH += ../../app ../../app/settings
SOURCES += settings_tests.cpp ../../app/settings/streamingpreferences.cpp
HEADERS += ../../app/settings/streamingpreferences.h
RESOURCES += settings.qrc
