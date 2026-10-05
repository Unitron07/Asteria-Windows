// Isolated Stage 2 probe policy. No Vulkan loader, SDL, or streaming dependency.
#pragma once
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace PyroWaveVulkan {
struct Extent { uint32_t width, height; };
inline Extent chooseExtent(Extent current, Extent requested, Extent minimum, Extent maximum) {
    if (!requested.width || !requested.height) return {0, 0};
    if (current.width != UINT32_MAX) return current;
    return {std::clamp(requested.width, minimum.width, maximum.width),
            std::clamp(requested.height, minimum.height, maximum.height)};
}
inline uint32_t imageCount(uint32_t minimum, uint32_t maximum) {
    if (!minimum || (maximum && maximum < minimum)) throw std::runtime_error("invalid image limits");
    const uint32_t desired = std::max(2u, minimum);
    return maximum ? std::min(desired, maximum) : desired;
}
enum class Mode { Immediate, Relaxed, Mailbox, Fifo };
inline Mode presentMode(bool vsync, const std::vector<Mode>& available) {
    const auto has = [&](Mode m) { return std::find(available.begin(), available.end(), m) != available.end(); };
    if (!vsync) for (auto m : {Mode::Immediate, Mode::Relaxed, Mode::Mailbox}) if (has(m)) return m;
    if (!has(Mode::Fifo)) throw std::runtime_error("FIFO missing");
    return Mode::Fifo;
}
struct QueueCandidate { bool graphics, compute, present; uint32_t count; };
inline int queueFamily(const std::vector<QueueCandidate>& candidates) {
    for (size_t i = 0; i < candidates.size(); ++i)
        if (candidates[i].count && candidates[i].graphics && candidates[i].compute && candidates[i].present)
            return static_cast<int>(i);
    return -1;
}
enum class Acquire { Ready, Suboptimal, NotReady, OutOfDate, Failed };
enum class Action { Submit, Retry, Rebuild, Suspend, Stop };
// A retry is serviced by the probe's SDL event wait, never immediate reposting.
struct Progress {
    bool rebuild = true, stopped = false;
    unsigned frameResource = 0;
    static constexpr unsigned resourceCount = 2;
    static constexpr int retryMilliseconds = 16;
    Action decide(Extent extent, bool commandComplete, Acquire result) {
        if (stopped) return Action::Stop;
        if (!extent.width || !extent.height) return Action::Suspend;
        if (rebuild) return Action::Rebuild;
        if (!commandComplete) return Action::Retry;
        switch (result) {
        case Acquire::Ready: return Action::Submit;
        case Acquire::Suboptimal: rebuild = true; return Action::Submit;
        case Acquire::NotReady: return Action::Retry;
        case Acquire::OutOfDate: rebuild = true; return Action::Rebuild;
        case Acquire::Failed: stopped = true; return Action::Stop;
        }
        throw std::runtime_error("invalid acquire result");
    }
    void submitted() { frameResource = (frameResource + 1) % resourceCount; }
};
} // namespace PyroWaveVulkan
