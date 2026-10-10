#pragma once
#include "pyrowave_vulkan_shared.h"
namespace Stage3 {
class Outputs {
    PyroWaveVulkan::Probe& owner;
    PyroWaveVulkan::Dispatch& vk;
    NativeDispatch& native;
    QueueLock& queueLock;
    PyroWaveVulkan::Log log;
    VkCommandPool pool{};
    VkCommandBuffer command{};
    VkPhysicalDeviceMemoryProperties memory{};
    struct Resource { VkImage image{}; VkBuffer buffer{}; VkDeviceMemory allocation{}; uint32_t memoryType{}; bool coherent=false; };
    struct Slot {
        std::array<Resource,3> images{}, staging{};
        VkSemaphore timeline{};
        Payloads payload;
        bool initialized=false;
    };
    std::array<Slot,SlotCount> slots{};
    Path path;
    bool diagnosticPrefill;
    void allocate(Resource& resource,VkMemoryRequirements requirements,VkMemoryPropertyFlags required,VkMemoryPropertyFlags preferred);
    void begin();
    void submit(VkSemaphore timeline,uint64_t wait,uint64_t signal);
    void wait(VkSemaphore timeline,uint64_t value);
public:
    Outputs(PyroWaveVulkan::Probe& o,PyroWaveVulkan::Dispatch& v,NativeDispatch& n,QueueLock& q,PyroWaveVulkan::Log l,Path p,bool prefill=false);
    ~Outputs();
    void initialize(const std::string& fault);
    pyrowave_gpu_buffers views(unsigned slot) const;
    pyrowave_gpu_sync_operation acquire(unsigned slot) const;
    pyrowave_gpu_sync_operation release(unsigned slot) const;
    void submitted(unsigned slot);
    PyroWave::Pixels read(unsigned slot,bool diagnosticDeviceIdle=false);
    void prefill(unsigned slot,uint8_t value);
    void drain();
    void close() noexcept;
};
}
