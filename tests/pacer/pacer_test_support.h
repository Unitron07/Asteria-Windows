#pragma once

// Only the surrounding application is substituted. pacer.cpp uses real Qt
// queues/locks/conditions, SDL threads/events and FFmpeg AVFrame/AVBuffer refs.
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>
#include <QMutex>
#include <atomic>
#include <chrono>
extern "C" {
#include <libavutil/frame.h>
#include <libavutil/mem.h>
}

constexpr int SDL_CODE_FRAME_READY = 0;
constexpr int RENDERER_ATTRIBUTE_NO_BUFFERING = 8;
constexpr int RENDERER_ATTRIBUTE_FORCE_PACING = 16;
struct VIDEO_STATS {
    uint32_t pacerDroppedFrames = 0, renderedFrames = 0;
    uint64_t totalPacerTimeUs = 0, totalRenderTimeUs = 0;
};
using PVIDEO_STATS = VIDEO_STATS*;
inline uint64_t LiGetMicroseconds() {
    return uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
class IFFmpegRenderer {
public:
    virtual ~IFFmpegRenderer() = default;
    virtual void renderFrame(AVFrame*) = 0;
    virtual void waitToRender() {}
    virtual void cleanupRenderContext() {}
    virtual bool isRenderThreadSupported() { return false; }
    virtual int getRendererAttributes() { return 0; }
    void prepareToRender() {}
    void acquireRenderLock() { lock.lock(); }
    void releaseRenderLock() { lock.unlock(); }
private:
    QMutex lock;
};
struct StreamUtils {
    static int getDisplayRefreshRate(SDL_Window*) { return 60; }
};
struct TestOverlayManager {
    void setOverlayRenderer(IFFmpegRenderer*) {}
};
struct Session {
    static Session* get() { static Session s; return &s; }
    TestOverlayManager& getOverlayManager() { return overlay; }
    TestOverlayManager overlay;
};
