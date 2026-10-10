#pragma once
// GPU/Qt-free capture core. No I/O, allocation, blocking queue or new mutex.
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <limits>

namespace PyroWavePerf {
constexpr uint64_t Max = std::numeric_limits<uint64_t>::max();
constexpr size_t BucketCount = 513;
inline uint64_t nowUs() noexcept {
    return uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
inline bool add(std::atomic<uint64_t>& target, uint64_t amount) noexcept {
    auto value = target.load(std::memory_order_relaxed);
    for (;;) {
        const bool overflow = amount > Max - value;
        const auto next = overflow ? Max : value + amount;
        if (target.compare_exchange_weak(value, next, std::memory_order_relaxed)) return !overflow;
    }
}
// Zero has its own bucket; eight subdivisions per power of two. Small integer
// values are exact. Percentiles use nearest rank and the bucket's upper bound.
inline size_t bucket(uint64_t value) noexcept {
    if (!value) return 0;
    unsigned exponent = 0;
    for (auto v = value; v >>= 1;) ++exponent;
    const auto base = uint64_t(1) << exponent;
    const auto fraction = exponent >= 3 ? (value - base) >> (exponent - 3) : (value - base) << (3 - exponent);
    return 1 + exponent * 8 + size_t(fraction);
}
inline uint64_t upper(size_t index) noexcept {
    if (!index) return 0;
    const unsigned exponent = unsigned((index - 1) / 8), part = unsigned((index - 1) % 8);
    const auto base = uint64_t(1) << exponent;
    if (exponent < 3) return base + ((part + 1) * base - 1) / 8;
    const auto step = base >> 3;
    return base + part * step + (step - 1);
}
struct Histogram {
    std::array<std::atomic<uint64_t>, BucketCount> bins{};
    std::atomic<uint64_t> count{0}, sum{0}, minimum{Max}, maximum{0};
    std::atomic<bool> overflow{false};
    void record(uint64_t value) noexcept {
        if (!add(bins[bucket(value)], 1) || !add(count, 1) || !add(sum, value)) overflow.store(true);
        auto low = minimum.load(std::memory_order_relaxed);
        while (value < low && !minimum.compare_exchange_weak(low, value, std::memory_order_relaxed)) {}
        auto high = maximum.load(std::memory_order_relaxed);
        while (value > high && !maximum.compare_exchange_weak(high, value, std::memory_order_relaxed)) {}
    }
    uint64_t percentile(unsigned percent) const noexcept {
        const auto n = count.load();
        if (!n || !percent || percent > 100) return 0;
        const auto rank = (n / 100) * percent + ((n % 100) * percent + 99) / 100;
        uint64_t seen = 0;
        for (size_t i = 0; i < BucketCount; ++i) {
            const auto next = bins[i].load();
            if (next >= rank - seen) return std::min(maximum.load(), std::max(minimum.load(), upper(i)));
            seen += next;
        }
        return maximum.load();
    }
};
enum Metric : size_t {
    Reassembly, Assembly, Preparation, DecoderQueue, GpuDecodeSubmission, CpuDecodeReadback,
    Interarrival, PublicationInterval, PresentationInterval, ScheduleInterval, PickupAge,
    RenderLoop, QueueSubmit, PresentCall, OverlayUpload, Recreation, Initialization, Cleanup,
    HostProcessing, Rtt, MetricCount
};
struct Definition { const char* name; const char* scope; const char* clock; };
inline constexpr std::array<Definition, MetricCount> Definitions{{
    {"networkReassembly", "CPU:first_packet_to_decode_unit_enqueue", "common-c monotonic"},
    {"frameAssembly", "CPU:transport_fragment_copy", "common-c monotonic"},
    {"parserPreparation", "CPU:parser_packet_push_output_setup", "steady_clock"},
    {"decoderQueueWait", "CPU:enqueue_to_submit_callback", "common-c monotonic"},
    {"gpuDecodeSubmission", "CPU:async_codec_decode_api", "steady_clock"},
    {"cpuDecodeReadback", "CPU:synchronous_codec_decode_and_I420_readback", "steady_clock"},
    {"frameInterarrival", "network:first_packet_of_successive_delivered_decode_units", "common-c monotonic"},
    {"decodePublicationInterval", "CPU:successive_output_publications", "steady_clock"},
    {"newFrameSubmissionInterval", "CPU:successive_new_video_submissions", "steady_clock"},
    {"renderSchedulingInterval", "CPU:successive_main_thread_render_entries", "steady_clock"},
    {"frameAgeAtPickup", "CPU:output_publication_to_renderer_pickup", "steady_clock"},
    {"renderLoop", "CPU:main_thread_render_entry_to_exit_including_stats_overlays_retries", "steady_clock"},
    {"vulkanQueueSubmitCall", "CPU:vkQueueSubmit_call_excluding_queue_mutex", "steady_clock"},
    {"vulkanPresentCall", "WSI:vkQueuePresentKHR_call_excluding_queue_mutex", "steady_clock"},
    {"overlayUploadCpu", "CPU:native_overlay_prepare_and_upload_command_recording", "steady_clock"},
    {"swapchainRecreation", "CPU:successful_recreation_including_existing_drain", "steady_clock"},
    {"decoderInitialization", "per-lifetime:decoder_initialize_including_provenance_preflight", "steady_clock"},
    {"decoderCleanup", "per-lifetime:decoder_destructor_resources_excluding_export", "steady_clock"},
    {"hostProcessing", "host:RTP_reported_processing_latency", "host"},
    {"estimatedRtt", "network:common-c_estimate_sampled_at_overlay_refresh", "common-c estimate"}
}};
enum Counter : size_t { Received, Decoded, Submitted, NetworkDrops, PresentationDrops,
    Rejected, RetainedRedraws, OverlayRedraws, VideoBytes, InvalidTimestamps, CounterCount };
inline constexpr std::array<const char*, CounterCount> CounterNames{{
    "receivedFrames", "decodedFrames", "newVideoSubmissions", "networkDrops", "presentationDrops",
    "rejectedFrames", "retainedFrameRedraws", "overlayOnlyRedraws", "videoPayloadBytes", "invalidTimestamps"
}};
enum Stage { Receive, Publish, Submit, Drop, Reject };
inline constexpr std::array<const char*, 5> StageNames{{"received", "published", "submitted", "dropped", "rejected"}};
struct Event { uint64_t relativeUs=0; uint32_t frame=0; int slot=-1; Stage stage=Receive; };
struct Capture {
    std::array<Histogram, MetricCount> metrics{};
    std::array<std::atomic<uint64_t>, CounterCount> counts{};
    std::atomic<bool> counterOverflow{false};
    static constexpr size_t EventCapacity = 512;
    std::array<Event, EventCapacity> events{};
    std::atomic<uint64_t> eventCount{0};
    std::array<std::array<char,512>,16> codecReports{};
    size_t codecReportCount=0;
    bool codecReportsTruncated=false;
    // Existing codec-report calls are serialized by the decoder lifetime.
    void codecReport(const char* text) noexcept {
        if(!text) return;
        if(codecReportCount==codecReports.size()) { codecReportsTruncated=true; return; }
        auto& out=codecReports[codecReportCount++];
        const auto length=std::strlen(text);
        std::memcpy(out.data(),text,std::min(length,out.size()-1));
        if(length>=out.size()) codecReportsTruncated=true;
    }
    uint64_t start=0, warmup=10000000, duration=60000000, end=0;
    // Each interval has a single writer: VideoDec or SDL/main, never both.
    uint64_t lastArrival=0, lastPublication=0, lastSubmission=0, lastSchedule=0;
    uint32_t lastFrame=0;
    bool haveLastFrame=false;
    void received(uint32_t frame) noexcept {
        if(!active(nowUs())) { haveLastFrame=false; return; }
        if(haveLastFrame) {
            const uint32_t delta=frame-lastFrame; // serial-number arithmetic, including wrap
            if(delta>1 && delta<0x80000000u) increment(NetworkDrops,delta-1);
        }
        haveLastFrame=true; lastFrame=frame; increment(Received); event(Receive,frame);
    }
    bool active(uint64_t now) const noexcept {
        return start && !end && now >= start && now-start >= warmup && now-start-warmup < duration;
    }
    void increment(Counter id, uint64_t amount=1) noexcept {
        if (!add(counts[id], amount)) counterOverflow.store(true);
    }
    void record(Metric id, uint64_t value) noexcept {
        if (active(nowUs())) metrics[id].record(value);
    }
    void interval(Metric id, uint64_t timestamp, uint64_t& previous) noexcept {
        if (!active(nowUs())) { previous=0; return; }
        if (previous) ordered(id, timestamp, previous);
        previous=timestamp;
    }
    void ordered(Metric id, uint64_t finish, uint64_t begin) noexcept {
        if (!active(nowUs())) return;
        if (finish < begin) increment(InvalidTimestamps);
        else metrics[id].record(finish-begin);
    }
    void event(Stage stage, uint32_t frame, int slot=-1) noexcept {
        const auto now=nowUs(); if (!active(now)) return;
        auto index=eventCount.load(std::memory_order_relaxed);
        for (;;) {
            if(index==Max) { counterOverflow.store(true); return; }
            if(eventCount.compare_exchange_weak(index,index+1,std::memory_order_relaxed)) break;
        }
        if (index < events.size()) events[size_t(index)]={now-start, frame, slot, stage};
    }
    void submitted(bool newFrame, bool overlay, uint32_t frame, int slot=-1) noexcept {
        if (!active(nowUs())) return;
        if (newFrame) {
            increment(Submitted); interval(PresentationInterval, nowUs(), lastSubmission); event(Submit, frame, slot);
        } else {
            increment(RetainedRedraws); if (overlay) increment(OverlayRedraws);
        }
    }
};
class Timer {
    Capture* capture; Metric metric; uint64_t begin;
public:
    Timer(Capture* c, Metric m) noexcept : capture(c), metric(m), begin(c ? nowUs() : 0) {}
    ~Timer() { if (capture) capture->record(metric, nowUs()-begin); }
};
class LifetimeTimer {
    std::atomic<uint64_t>* destination; uint64_t begin;
public:
    explicit LifetimeTimer(std::atomic<uint64_t>* result) noexcept : destination(result), begin(result ? nowUs() : 0) {}
    ~LifetimeTimer() { if(destination) destination->store(nowUs()-begin,std::memory_order_relaxed); }
};
static_assert(std::atomic<uint64_t>::is_always_lock_free, "capture requires lock-free 64-bit counters");
}
