# Experimental codec, bundled in normal Windows app builds. Restricted runtime
# load only when explicitly selected; no DLL or Vulkan startup import.
pyrowave_experimental {
    !win32: error("The PyroWave P0 runtime is Windows only")
    isEmpty(PYROWAVE_ROOT): error("Set PYROWAVE_ROOT to the matching target install directory")
    isEmpty(VULKAN_HEADERS): error("Set VULKAN_HEADERS to the pinned Vulkan-Headers include directory")
    !exists($$PYROWAVE_ROOT/include/pyrowave/pyrowave.h): error("Missing pinned PyroWave header")
    !exists($$VULKAN_HEADERS/vulkan/vulkan_core.h): error("Missing pinned Vulkan header")
    DEFINES += PYROWAVE_EXPERIMENTAL=1
    INCLUDEPATH += $$PYROWAVE_ROOT/include $$VULKAN_HEADERS
    SOURCES += $$PWD/pyrowave_frame.cpp $$PWD/pyrowave_runtime.cpp
    HEADERS += $$PWD/pyrowave_frame.h $$PWD/pyrowave_runtime.h
    # Offline probe includes this pri too; only the actual app owns Session/Qt.
    equals(TARGET, Asteria) {
        SOURCES += $$PWD/pyrowave_decoder.cpp $$PWD/pyrowave_sdl.cpp $$PWD/pyrowave_gpu.cpp
        HEADERS += $$PWD/pyrowave_decoder.h $$PWD/pyrowave_pixels.h $$PWD/pyrowave_sdl.h $$PWD/pyrowave_gpu.h $$PWD/pyrowave_queue.h $$PWD/pyrowave_stats.h
    }
}
