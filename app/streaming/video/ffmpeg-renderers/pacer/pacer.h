#pragma once

#ifdef PACER_TEST
#include "pacer_test_support.h"
#else
#include "../../decoder.h"
#include "../renderer.h"
#endif

#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
#include <atomic>

// The maximum number of frames pacer will ever hold is:
// - 3 frames in each of the pacing and rendering queues
// - 1 frame rendering (or being released by render-side catch-up)
// - 1 frame for deferred free
// - 1 frame being released by V-sync catch-up, outside the queue lock
// The caller's incoming decoded frame is not yet owned by these queues.
#define PACER_MAX_QUEUED_FRAMES 3
#define PACER_MAX_OUTSTANDING_FRAMES (2 * PACER_MAX_QUEUED_FRAMES + 1 + 1 + 1)

class IVsyncSource {
public:
    virtual ~IVsyncSource() {}
    virtual bool initialize(SDL_Window* window, int displayFps) = 0;

    // Asynchronous sources produce callbacks on their own, while synchronous
    // sources require calls to waitForVsync().
    virtual bool isAsync() = 0;

    virtual bool waitForVsync() {
        // Synchronous sources must implement waitForVsync()!
        SDL_assert(false);
        return false;
    }
};

class Pacer
{
public:
    Pacer(IFFmpegRenderer* renderer, PVIDEO_STATS videoStats);

    ~Pacer();

    void submitFrame(AVFrame* frame);

    bool initialize(SDL_Window* window, int maxVideoFps, bool enablePacing);

    void signalVsync();

    void renderOnMainThread();

    // Protect FFDecoder's window snapshot/reset against Pacer's stats writers.
    QMutex* videoStatsLock() { return &m_VideoStatsLock; }

private:
#ifdef PACER_TEST
    friend struct PacerTestPeer;
#endif
    static int vsyncThread(void* context);

    static int renderThread(void* context);

    void handleVsync(int timeUntilNextVsyncMillis);

    void enqueueFrameForRenderingAndUnlock(AVFrame* frame);

    void renderFrame(AVFrame* frame);

    void dropFrameForEnqueue(QQueue<AVFrame*>& queue);

    QQueue<AVFrame*> m_RenderQueue;
    QQueue<AVFrame*> m_PacingQueue;
    QQueue<int> m_PacingQueueHistory;
    QQueue<int> m_RenderQueueHistory;
    QMutex m_FrameQueueLock;
    QMutex m_VideoStatsLock;
    QWaitCondition m_RenderQueueNotEmpty;
    QWaitCondition m_PacingQueueNotEmpty;
    QWaitCondition m_VsyncSignalled;
    SDL_Thread* m_RenderThread;
    SDL_Thread* m_VsyncThread;
    AVFrame* m_DeferredFreeFrame;
    std::atomic<bool> m_Stopping;

    IVsyncSource* m_VsyncSource;
    IFFmpegRenderer* m_VsyncRenderer;
    int m_MaxVideoFps;
    int m_DisplayFps;
    PVIDEO_STATS m_VideoStats;
    int m_RendererAttributes;
};
