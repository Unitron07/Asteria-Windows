QT -= gui
QT += core
TEMPLATE = app
TARGET = pyrowave-offline-proof
CONFIG += console c++17 pyrowave_experimental
CONFIG -= app_bundle
SOURCES += $$PWD/offline_proof.cpp
INCLUDEPATH += $$PWD/../../app/streaming/video
include(../../app/streaming/video/pyrowave_experimental.pri)
