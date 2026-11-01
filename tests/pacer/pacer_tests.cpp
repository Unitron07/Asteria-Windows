#include "pacer.h"
#include <QMutexLocker>
#include <algorithm>
#include <condition_variable>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " << #x << '\n'; std::abort(); } } while (0)

// Every blocking operation has a finite watchdog; ordering comes from gates,
// never sleeps, frame-rate timing or assumptions about thread scheduling.
struct Gate {
    std::mutex mutex;
    std::condition_variable cv;
    bool entered = false, released = false;
    void block() {
        std::unique_lock<std::mutex> lock(mutex);
        entered = true; cv.notify_all();
        CHECK(cv.wait_for(lock, std::chrono::seconds(10), [&] { return released; }));
    }
    void wait() {
        std::unique_lock<std::mutex> lock(mutex);
        CHECK(cv.wait_for(lock, std::chrono::seconds(10), [&] { return entered; }));
    }
    void open() {
        std::lock_guard<std::mutex> lock(mutex);
        released = true; cv.notify_all();
    }
};
struct Ledger {
    struct Entry { Ledger* owner; int id; Gate* releaseGate; };
    std::mutex mutex;
    std::vector<bool> freed;
    int live = 0, peak = 0;
    AVFrame* frame(Gate* releaseGate = nullptr) {
        auto* f = av_frame_alloc(); CHECK(f);
        int id;
        {
            std::lock_guard<std::mutex> lock(mutex);
            id = int(freed.size()); freed.push_back(false);
            peak = std::max(peak, ++live);
        }
        auto* data = static_cast<uint8_t*>(av_malloc(1)); CHECK(data);
        f->buf[0] = av_buffer_create(data, 1, [](void* opaque, uint8_t* bytes) {
            std::unique_ptr<Entry> entry(static_cast<Entry*>(opaque));
            if (entry->releaseGate) entry->releaseGate->block();
            std::lock_guard<std::mutex> lock(entry->owner->mutex);
            CHECK(!entry->owner->freed[entry->id]);
            entry->owner->freed[entry->id] = true;
            --entry->owner->live; av_free(bytes);
        }, new Entry{this, id, releaseGate}, 0);
        CHECK(f->buf[0]); f->pts = id; f->pkt_dts = int64_t(LiGetMicroseconds());
        return f;
    }
    int count() { std::lock_guard<std::mutex> lock(mutex); return live; }
    bool released(int id) { std::lock_guard<std::mutex> lock(mutex); return freed.at(id); }
    ~Ledger() { CHECK(count() == 0); }
};
struct Renderer : IFFmpegRenderer {
    Gate* stall = nullptr;
    bool threaded = false;
    int attributes = 0;
    std::atomic<int> rendered{0}, cleaned{0};
    std::vector<int> ids;
    std::function<void()> onCleanup;
    void renderFrame(AVFrame* f) override {
        CHECK(f && f->buf[0] && av_buffer_get_ref_count(f->buf[0]) > 0);
        ids.push_back(int(f->pts));
        ++rendered;
        if (stall) stall->block();
        CHECK(f->buf[0]->data);
    }
    bool isRenderThreadSupported() override { return threaded; }
    int getRendererAttributes() override { return attributes; }
    void cleanupRenderContext() override { if (onCleanup) onCleanup(); ++cleaned; }
};
struct ManualVsync : IVsyncSource {
    bool initialize(SDL_Window*, int) override { return true; }
    bool isAsync() override { return true; }
};
struct PacerTestPeer {
    static void paced(Pacer& p) { p.m_VsyncSource = new ManualVsync; }
    static void tick(Pacer& p, int deadline = 3) { p.handleVsync(deadline); }
    static void strictPacing(Pacer& p) { p.m_MaxVideoFps = 30; }
    static void queues(Pacer& p, int pacing, int rendering) {
        QMutexLocker lock(&p.m_FrameQueueLock);
        CHECK(p.m_PacingQueue.size() == pacing);
        CHECK(p.m_RenderQueue.size() == rendering);
    }
    static void vsyncWorker(Pacer& p) {
        p.m_VsyncThread = SDL_CreateThread(Pacer::vsyncThread, "test-vsync", &p);
        CHECK(p.m_VsyncThread);
    }
};

void stalledAggregate() {
    Ledger ledger; Renderer renderer; VIDEO_STATS stats{};
    auto p = std::make_unique<Pacer>(&renderer, &stats);
    CHECK(p->initialize(nullptr, 120, false));
    PacerTestPeer::paced(*p);
    p->submitFrame(ledger.frame()); PacerTestPeer::tick(*p); p->renderOnMainThread();
    CHECK(ledger.count() == 1); // deferred, protected through the next render
    p->submitFrame(ledger.frame()); PacerTestPeer::tick(*p);
    Gate renderGate; renderer.stall = &renderGate;
    std::thread rendering([&] { p->renderOnMainThread(); }); renderGate.wait();
    for (int i = 0; i < 3; ++i) { p->submitFrame(ledger.frame()); PacerTestPeer::tick(*p); }
    Gate releaseGate;
    p->submitFrame(ledger.frame(&releaseGate));
    p->submitFrame(ledger.frame()); p->submitFrame(ledger.frame());
    PacerTestPeer::queues(*p, 3, 3);
    CHECK(ledger.count() == 8); // already exceeds the old advertised budget of 5
    PacerTestPeer::strictPacing(*p);
    std::thread vsync([&] { PacerTestPeer::tick(*p); }); releaseGate.wait();
    // A slow av_buffer finalizer does not own the mutex. Refill while it stalls.
    p->submitFrame(ledger.frame());
    PacerTestPeer::queues(*p, 3, 3);
    CHECK(ledger.count() == 9);
    CHECK(PACER_MAX_OUTSTANDING_FRAMES >= ledger.count());
    CHECK(!ledger.released(0)); // deferred must survive the stalled GPU consumer
    releaseGate.open(); vsync.join();
    renderGate.open(); rendering.join(); renderer.stall = nullptr;
    CHECK(ledger.released(0));
    CHECK(stats.renderedFrames == 2);
    p.reset(); CHECK(ledger.count() == 0); CHECK(renderer.cleaned == 1);
    std::cout << "PASS aggregate: 6 queued + render + deferred + VBlank release = 9 (old budget 5)\n";
}

void saturationAndLifetime(bool pacing) {
    Ledger ledger; Renderer renderer; VIDEO_STATS stats{};
    {
        Pacer p(&renderer, &stats); CHECK(p.initialize(nullptr, 60, false));
        if (pacing) PacerTestPeer::paced(p);
        auto* first = ledger.frame();
        auto* external = av_buffer_ref(first->buf[0]); CHECK(external);
        p.submitFrame(first);
        for (int i = 0; i < 4; ++i) p.submitFrame(ledger.frame());
        CHECK(stats.pacerDroppedFrames == 2); // oldest replacement counted once
        CHECK(!ledger.released(0)); CHECK(ledger.released(1));
        av_buffer_unref(&external); CHECK(ledger.released(0));
        if (pacing) PacerTestPeer::tick(p);
        p.renderOnMainThread();
        CHECK(renderer.ids.front() == (pacing ? 4 : 2));
        const auto drops = stats.pacerDroppedFrames;
        for (int i = 0; i < 5; ++i) p.renderOnMainThread(); // stale notifications
        CHECK(stats.pacerDroppedFrames == drops);
    }
    CHECK(ledger.count() == 0);
}

void vblankAndHistory() {
    Ledger ledger; Renderer renderer; VIDEO_STATS stats{};
    Pacer p(&renderer, &stats); CHECK(p.initialize(nullptr, 120, false));
    PacerTestPeer::paced(p);
    // Zero/late VBlank and TIMER_SLACK must never become an unbounded wait.
    for (int deadline : {0, 1, 3, 8, 16}) PacerTestPeer::tick(p, deadline);
    CHECK(stats.renderedFrames == 0 && stats.pacerDroppedFrames == 0);
    for (int i = 0; i < 3; ++i) p.submitFrame(ledger.frame());
    PacerTestPeer::tick(p); // previous empty ticks allow a lenient target of 3
    PacerTestPeer::queues(p, 2, 1); CHECK(stats.pacerDroppedFrames == 0);
    p.renderOnMainThread(); CHECK(renderer.ids.front() == 0);
}

void concurrentAndReconnect(bool pacing, int attributes) {
    for (int cycle = 0; cycle < 12; ++cycle) {
        Ledger ledger; Renderer renderer; VIDEO_STATS stats{};
        renderer.threaded = true; renderer.attributes = attributes;
        uint64_t savedDrops = 0, savedRendered = 0;
        int unconsumed = 0;
        renderer.onCleanup = [&] {
            // Workers have stopped; only queued frames and the last displayed
            // reference remain. Destructor draining is not a policy drop.
            unconsumed = ledger.count() - (renderer.rendered > 0 ? 1 : 0);
        };
        {
            Pacer p(&renderer, &stats); CHECK(p.initialize(nullptr, 120, false));
            if (pacing) PacerTestPeer::paced(p);
            // Exercise concurrent producer, VBlank, renderer and stats rollover.
            std::atomic<bool> done{false};
            std::thread snapshot([&] {
                while (!done) {
                    QMutexLocker lock(p.videoStatsLock());
                    savedDrops += stats.pacerDroppedFrames; savedRendered += stats.renderedFrames;
                    stats = {};
                }
            });
            std::thread producer([&] { for (int i = 0; i < 300; ++i) p.submitFrame(ledger.frame()); });
            if (pacing) for (int i = 0; i < 300; ++i) PacerTestPeer::tick(p);
            producer.join();
            done = true; snapshot.join();
            // Teardown joins workers before releasing frames and renderer context.
        }
        CHECK(savedDrops + stats.pacerDroppedFrames + savedRendered + stats.renderedFrames + unconsumed == 300);
        CHECK(savedRendered + stats.renderedFrames == uint64_t(renderer.rendered));
        CHECK(ledger.count() == 0); CHECK(renderer.cleaned == 1);
    }
}

void teardownWorkers() {
    for (int i = 0; i < 40; ++i) {
        Ledger ledger; Renderer renderer; VIDEO_STATS stats{}; renderer.threaded = true;
        {
            Pacer p(&renderer, &stats); CHECK(p.initialize(nullptr, 60, false));
            PacerTestPeer::paced(p); PacerTestPeer::vsyncWorker(p);
            if (i % 2) p.submitFrame(ledger.frame());
            p.signalVsync();
        }
        CHECK(ledger.count() == 0); CHECK(renderer.cleaned == 1);
    }
}

int main() {
    SDL_SetMainReady(); CHECK(SDL_Init(SDL_INIT_TIMER | SDL_INIT_EVENTS) == 0);
    stalledAggregate();
    saturationAndLifetime(false); saturationAndLifetime(true);
    vblankAndHistory();
    for (bool pacing : {false, true}) for (int attrs : {0, RENDERER_ATTRIBUTE_NO_BUFFERING})
        concurrentAndReconnect(pacing, attrs);
    teardownWorkers(); SDL_Quit();
    std::cout << "PASS saturation/drop statistics/reference lifetime/VBlank/history/concurrency/reconnect/teardown/pacing on-off\n";
}
