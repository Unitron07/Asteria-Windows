#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace Stage3 {
constexpr unsigned SlotCount = 3;
enum class Path { Compute, Fragment };
// Vulkan bit values, without a loader/header dependency in the policy executable.
inline uint32_t usage(Path path) { return 1u | (path == Path::Compute ? 8u : 16u); }
struct Plane { uint32_t width, height; uint64_t bytes; };
inline std::array<Plane,3> planes(uint32_t w,uint32_t h) {
    if (w < 128 || h < 128 || w > 4096 || h > 4096 || (w|h)&1 || uint64_t(w)*h > 3840ull*2160)
        throw std::runtime_error("unsupported SDR 420 extent");
    return {{{w,h,uint64_t(w)*h},{w/2,h/2,uint64_t(w/2)*(h/2)},{w/2,h/2,uint64_t(w/2)*(h/2)}}};
}
struct Capabilities {
    bool api12, timeline, sync2, subgroupSizeControl, computeFullSubgroups, subgroupRange;
    bool subgroupOperations, storage8, largeTexelBuffers, writeWithoutFormat;
};
inline bool supported(const Capabilities& c) {
    return c.api12 && c.timeline && c.sync2 && c.subgroupSizeControl && c.computeFullSubgroups &&
        c.subgroupRange && c.subgroupOperations && (c.storage8 || c.largeTexelBuffers) && c.writeWithoutFormat;
}
inline bool identical(uintptr_t expectedInstance,uintptr_t expectedPhysical,uintptr_t expectedDevice,
                      uintptr_t instance,uintptr_t physical,uintptr_t device) {
    return expectedInstance && expectedPhysical && expectedDevice && expectedInstance == instance &&
        expectedPhysical == physical && expectedDevice == device;
}
struct Payloads {
    // Initial transition is signaled by caller submission at 1. Never host-signaled.
    uint64_t consumed = 1, decoded = 0;
    bool pending = false;
    std::array<uint64_t,3> next() const {
        if (pending || consumed > std::numeric_limits<uint64_t>::max()-2)
            throw std::runtime_error("pending consumer or timeline overflow");
        return {consumed,consumed+1,consumed+2};
    }
    void submitted(uint64_t value) {
        if (value != next()[1]) throw std::runtime_error("invalid decode payload");
        decoded = value; pending = true;
    }
    void complete(uint64_t value) {
        if (!pending || value != decoded+1) throw std::runtime_error("invalid consumer payload");
        consumed = value; pending = false;
    }
};
class QueueLock {
    std::mutex mutex;
    inline static thread_local QueueLock* held = nullptr;
public:
    std::atomic<uint64_t> codecLocks{0}, codecUnlocks{0}, callerLocks{0}, callerUnlocks{0};
    std::atomic<size_t> lastCodecThread{0};
    void lock(bool codec) {
        if (held) throw std::logic_error("recursive queue lock assumption");
        mutex.lock(); held = this;
        if (codec) { ++codecLocks; lastCodecThread = std::hash<std::thread::id>{}(std::this_thread::get_id()); }
        else ++callerLocks;
    }
    void unlock(bool codec) {
        if (held != this) throw std::logic_error("unbalanced queue unlock");
        if (codec) ++codecUnlocks; else ++callerUnlocks;
        held = nullptr; mutex.unlock();
    }
    bool balanced() const { return codecLocks == codecUnlocks && callerLocks == callerUnlocks; }
    struct Guard {
        QueueLock& owner;
        explicit Guard(QueueLock& q):owner(q) { owner.lock(false); }
        ~Guard() { owner.unlock(false); }
    };
};
}
