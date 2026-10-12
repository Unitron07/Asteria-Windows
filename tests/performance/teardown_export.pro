QT += core gui qml
TEMPLATE = app
TARGET = pyrowave-perf-teardown-tests
CONFIG += console c++17 pyrowave_experimental
CONFIG -= app_bundle
VIDEO = $$PWD/../../app/streaming/video
# The Session boundary stub must precede app/. No production decoder/resource
# implementations are stubbed or copied.
INCLUDEPATH += $$PWD/stubs $$PWD/../../app $$VIDEO
INCLUDEPATH += $$PWD/../../moonlight-common-c/moonlight-common-c/src
INCLUDEPATH += $$SDL_ROOT/include/$$SDL_ARCH/SDL2
DEFINES += SDL_MAIN_HANDLED PYROWAVE_VULKAN_SHARED_DEVICE PYROWAVE_VULKAN_STAGE4
SOURCE_SHA = $$system(git rev-parse HEAD)
isEmpty(SOURCE_SHA): error("Cannot determine performance regression source revision")
DEFINES += ASTERIA_PYROWAVE_SOURCE_REVISION=\\\"$$SOURCE_SHA\\\"
isEmpty(DECODER_SOURCE): DECODER_SOURCE = $$VIDEO/pyrowave_decoder.cpp
SOURCES += $$PWD/teardown_export_tests.cpp $$DECODER_SOURCE
SOURCES += $$VIDEO/pyrowave_sdl.cpp $$VIDEO/pyrowave_gpu.cpp
SOURCES += $$VIDEO/pyrowave_vulkan_dispatch.cpp $$VIDEO/pyrowave_vulkan_probe.cpp
SOURCES += $$VIDEO/pyrowave_vulkan_shared.cpp $$VIDEO/pyrowave_vulkan_presenter.cpp
SOURCES += $$VIDEO/pyrowave_vulkan_overlays.cpp $$VIDEO/pyrowave_vulkan_live.cpp
include(../../app/streaming/video/pyrowave_experimental.pri)
LIBS += -L$$SDL_ROOT/lib/$$SDL_ARCH -lSDL2 -luser32
