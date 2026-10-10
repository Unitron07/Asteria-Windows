#pragma once
#include "pyrowave_vulkan_dispatch.h"
#include "pyrowave_vulkan_policy.h"
#include <SDL.h>
#include <array>
#include <atomic>
#include <functional>
#include <vector>

namespace PyroWaveVulkan {
using Log = std::function<void(const std::string&)>;
struct ProbeOptions {
    bool vsync = true, validation = true, api10 = false;
    std::string failAt, deviceName;
    // Offline Stage 3 hooks: no swapchain or synthetic drawing resources.
    // Callback-owned feature storage must outlive this owner and the codec wrapper.
    bool existingWin32Window = false;
    std::function<void()> lockQueue, unlockQueue;
    bool deviceOnly = false;
    uint32_t minimumApi = VK_API_VERSION_1_0;
    std::function<bool(VkPhysicalDevice)> suitable;
    std::function<void(VkDeviceCreateInfo&)> configure;
#ifdef PYROWAVE_VULKAN_STAGE4
    // Qualified video hooks shared by Stage 4 and production native PyroWave.
    std::function<void(VkCommandBuffer)> beforeVideo;
    std::function<void(VkCommandBuffer,VkRenderPass,VkExtent2D)> video;
    std::function<void(VkSubmitInfo&)> videoSubmit;
    std::function<void()> videoSubmitted;
#endif
};
// Reusable low-level synthetic owner, deliberately not IVideoDecoder/Session.
class Probe {
public:
    Probe(SDL_Window* window, Dispatch& dispatch, Log log, ProbeOptions options);
    ~Probe();
    Probe(const Probe&) = delete;
    Probe& operator=(const Probe&) = delete;
    void initialize();
    bool draw(); // false = finite retry/suspended, true = submission/present accepted
    void resize() { progress.rebuild = true; }
    void close() noexcept;
    uint64_t presents = 0, recreations = 0;
    VkFormat swapchainFormat = VK_FORMAT_UNDEFINED;
#ifdef PYROWAVE_VULKAN_STAGE4
    double queueSubmitMs = 0, presentCallMs = 0;
#endif
    std::atomic<unsigned> validationErrors{0};
    std::atomic<unsigned> validationWarnings{0};
    bool validationActive = false;
    bool cleanupOkay = true;
    bool injectedFailure = false;
    std::string selectedDevice;
    VkInstance instanceHandle() const { return instance; }
    VkPhysicalDevice physicalHandle() const { return physical; }
    VkDevice deviceHandle() const { return device; }
    VkQueue queueHandle() const { return queue; }
    uint32_t queueFamilyIndex() const { return family; }
    const VkInstanceCreateInfo& instanceCreateInfo() const { return instanceInfo; }
    const VkDeviceCreateInfo& deviceCreateInfo() const { return deviceInfo; }
private:
#if defined(STAGE4_RESOURCE_TEST) || defined(STAGE5_RESOURCE_TEST)
    friend struct ProbeTestAccess;
#endif
    SDL_Window* window;
    Dispatch& vk;
    Log log;
    ProbeOptions options;
    Progress progress;
    DWORD thread;
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    uint32_t family = 0;
    VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkExtent2D extent{};
    struct Frame {
        VkCommandBuffer command = VK_NULL_HANDLE;
        VkSemaphore acquired = VK_NULL_HANDLE;
        VkFence complete = VK_NULL_HANDLE, acquireComplete = VK_NULL_HANDLE;
        bool acquisitionPending = false;
    };
    std::array<Frame, Progress::resourceCount> frames{};
    struct Image { VkImage image = VK_NULL_HANDLE; VkImageView view = VK_NULL_HANDLE; VkFramebuffer framebuffer = VK_NULL_HANDLE; VkSemaphore presented = VK_NULL_HANDLE; };
    std::vector<Image> images;
    // Stable nested storage, suitable for a future reviewed borrowed-device API.
    VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    VkInstanceCreateInfo instanceInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    VkPhysicalDeviceFeatures features{};
    float priority = 1.0f;
    std::vector<const char*> instanceExtensions, deviceExtensions, layers;
    VkDebugUtilsMessengerCreateInfoEXT debugInfo{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    VkValidationFeatureEnableEXT syncValidation = VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;
    VkValidationFeaturesEXT validationInfo{VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};
    void mainThread() const;
    void drawableSize(int& width,int& height) const;
    VkResult queueOperation(const std::function<VkResult()>& operation);
    void checkpoint(const char* name);
    void selectDevice();
    bool recreate();
    void destroySwapchain() noexcept;
    void record(VkCommandBuffer command, uint32_t image);
    static VKAPI_ATTR VkBool32 VKAPI_CALL validation(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT* data, void* user);
};
} // namespace PyroWaveVulkan
