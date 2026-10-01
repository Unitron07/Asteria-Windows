QT -= gui
QT += core
TEMPLATE = app
TARGET = pyrowave-offline-proof
CONFIG += console c++17 pyrowave_experimental
CONFIG -= app_bundle
SOURCES += $$PWD/offline_proof.cpp
INCLUDEPATH += $$PWD/../../app/streaming/video
include(../../app/streaming/video/pyrowave_experimental.pri)
contains(CONFIG, pyrowave_sdl) {
    !contains(SDL_ARCH, ^(x64|arm64)$): error("Set SDL_ARCH=x64 or arm64")
    DEFINES += PYROWAVE_SDL_PRESENTATION=1
    SOURCES += $$PWD/sdl_presentation.cpp
    INCLUDEPATH += $$SDL_ROOT/include/$$SDL_ARCH/SDL2
    LIBS += -L$$SDL_ROOT/lib/$$SDL_ARCH -lSDL2 -lbcrypt
}
