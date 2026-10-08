#pragma once
#include "pyrowave_vulkan_probe.h"
#include "pyrowave_vulkan_shared_policy.h"
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
}
