#pragma once
#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <windows.h>
#include <vulkan/vulkan.h>
#include <string>
#include <stdexcept>

namespace PyroWaveVulkan {
// Explicit dispatch, owned by the SDL main thread. No vulkan-1 import library.
class Dispatch {
public:
    Dispatch() = default;
    ~Dispatch();
    Dispatch(const Dispatch&) = delete;
    Dispatch& operator=(const Dispatch&) = delete;
    void load();
    void instance(VkInstance instance);
    void device(VkDevice device);
    std::string loaderPath;
    // Test injection deliberately excludes destruction functions.
    std::string missingExport;
    PFN_vkGetInstanceProcAddr GetInstanceProcAddr = nullptr;
    PFN_vkGetDeviceProcAddr GetDeviceProcAddr = nullptr;
    PFN_vkEnumerateInstanceVersion EnumerateInstanceVersion = nullptr;
#define PW_GLOBAL(X) X(CreateInstance) X(EnumerateInstanceExtensionProperties) X(EnumerateInstanceLayerProperties)
#define PW_INSTANCE(X) X(DestroyInstance) X(EnumeratePhysicalDevices) X(GetPhysicalDeviceProperties) \
    X(GetPhysicalDeviceQueueFamilyProperties) X(GetPhysicalDeviceSurfaceSupportKHR) \
    X(GetPhysicalDeviceSurfaceCapabilitiesKHR) X(GetPhysicalDeviceSurfaceFormatsKHR) \
    X(GetPhysicalDeviceSurfacePresentModesKHR) X(EnumerateDeviceExtensionProperties) \
    X(CreateDevice) X(DestroyDevice) X(DestroySurfaceKHR)
#define PW_DEVICE(X) X(GetDeviceQueue) X(DeviceWaitIdle) X(CreateSwapchainKHR) X(DestroySwapchainKHR) \
    X(GetSwapchainImagesKHR) X(AcquireNextImageKHR) X(QueuePresentKHR) X(CreateImageView) X(DestroyImageView) \
    X(CreateRenderPass) X(DestroyRenderPass) X(CreateFramebuffer) X(DestroyFramebuffer) \
    X(CreateSemaphore) X(DestroySemaphore) X(CreateFence) X(DestroyFence) X(GetFenceStatus) X(ResetFences) X(WaitForFences) \
    X(CreateCommandPool) X(DestroyCommandPool) X(AllocateCommandBuffers) X(ResetCommandBuffer) \
    X(BeginCommandBuffer) X(EndCommandBuffer) X(CmdBeginRenderPass) X(CmdClearAttachments) X(CmdEndRenderPass) X(QueueSubmit)
#define PW_DECLARE(name) PFN_vk##name name = nullptr;
    PW_GLOBAL(PW_DECLARE)
    PW_INSTANCE(PW_DECLARE)
    PW_DEVICE(PW_DECLARE)
#undef PW_DECLARE
    PFN_vkGetPhysicalDeviceProperties2 GetPhysicalDeviceProperties2 = nullptr;
    PFN_vkCreateDebugUtilsMessengerEXT CreateDebugUtilsMessengerEXT = nullptr;
    PFN_vkDestroyDebugUtilsMessengerEXT DestroyDebugUtilsMessengerEXT = nullptr;
private:
    HMODULE module = nullptr;
    PFN_vkVoidFunction required(PFN_vkVoidFunction value, const char* name) const;
};
inline void check(VkResult result, const char* operation) {
    if (result != VK_SUCCESS) throw std::runtime_error(std::string(operation) + " VkResult=" + std::to_string(result));
}
} // namespace PyroWaveVulkan
