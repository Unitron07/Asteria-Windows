#pragma once
#include "pyrowave_vulkan_probe.h"
#include "shared_policy.h"
#include "pyrowave_runtime.h"

namespace Stage3 {
#define STAGE3_INSTANCE(X) X(GetPhysicalDeviceFeatures2) X(GetPhysicalDeviceImageFormatProperties) X(GetPhysicalDeviceMemoryProperties) X(GetPhysicalDeviceFormatProperties)
#define STAGE3_DEVICE(X) X(CreateImage) X(DestroyImage) X(GetImageMemoryRequirements) X(AllocateMemory) X(FreeMemory) X(BindImageMemory) X(CreateBuffer) X(DestroyBuffer) X(GetBufferMemoryRequirements) X(BindBufferMemory) X(MapMemory) X(UnmapMemory) X(InvalidateMappedMemoryRanges) X(CmdPipelineBarrier) X(CmdCopyImageToBuffer) X(CmdClearColorImage) X(WaitSemaphores)
struct NativeDispatch {
#define DECLARE(name) PFN_vk##name name = nullptr;
    STAGE3_INSTANCE(DECLARE)
    STAGE3_DEVICE(DECLARE)
#undef DECLARE
    void loadInstance(PyroWaveVulkan::Dispatch& vk,VkInstance instance);
    void loadDevice(PyroWaveVulkan::Dispatch& vk,VkDevice device);
};
// Member chains, vectors, and queue callback userdata never move during borrowing.
struct Requirements {
    NativeDispatch& native;
    PyroWaveVulkan::Dispatch& vk;
    PyroWaveVulkan::Log log;
    VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    VkPhysicalDeviceVulkan11Features f11{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
    VkPhysicalDeviceVulkan12Features f12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    VkPhysicalDeviceSubgroupSizeControlFeatures subgroup{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES};
    VkPhysicalDeviceSynchronization2Features sync2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES};
    std::array<const char*,2> extensions{VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME};
    bool suitable(VkPhysicalDevice physical);
    void configure(VkDeviceCreateInfo& info);
};
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
