#include "pyrowave_vulkan_probe.h"
#include <SDL_vulkan.h>
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>
#ifdef PYROWAVE_VULKAN_STAGE4
#include <chrono>
#endif

namespace PyroWaveVulkan {
namespace {
std::string version(uint32_t v) {
    return std::to_string(VK_VERSION_MAJOR(v)) + "." + std::to_string(VK_VERSION_MINOR(v)) + "." + std::to_string(VK_VERSION_PATCH(v));
}
bool has(const std::vector<VkExtensionProperties>& items, const char* name) {
    return std::any_of(items.begin(), items.end(), [&](const auto& p) { return !std::strcmp(p.extensionName, name); });
}
std::string hexBytes(const uint8_t* bytes, size_t length) {
    std::ostringstream s;
    s << std::hex << std::setfill('0');
    for (size_t i = 0; i < length; ++i) s << std::setw(2) << unsigned(bytes[i]);
    return s.str();
}
VkPresentModeKHR nativeMode(Mode mode) {
    switch (mode) {
    case Mode::Immediate: return VK_PRESENT_MODE_IMMEDIATE_KHR;
    case Mode::Relaxed: return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    case Mode::Mailbox: return VK_PRESENT_MODE_MAILBOX_KHR;
    case Mode::Fifo: return VK_PRESENT_MODE_FIFO_KHR;
    }
    throw std::runtime_error("invalid present mode");
}
}
Probe::Probe(SDL_Window* w, Dispatch& d, Log logger, ProbeOptions o)
    : window(w), vk(d), log(std::move(logger)), options(std::move(o)), thread(GetCurrentThreadId()) {}
Probe::~Probe() { close(); }
void Probe::mainThread() const {
    if (thread != GetCurrentThreadId()) throw std::runtime_error("Vulkan/WSI call outside owning SDL main thread");
}
void Probe::checkpoint(const char* name) {
    if (options.failAt == name) {
        injectedFailure = true;
        throw std::runtime_error(std::string("injected failure after ") + name);
    }
}
VKAPI_ATTR VkBool32 VKAPI_CALL Probe::validation(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* data, void* user) {
    auto* self = static_cast<Probe*>(user);
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ++self->validationErrors;
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) ++self->validationWarnings;
    self->log(std::string("VALIDATION ") + (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT ? "ERROR " : "WARNING ") + data->pMessage);
    return VK_FALSE;
}
void Probe::initialize() {
    mainThread();
    uint32_t loaderVersion = VK_API_VERSION_1_0;
    if (vk.EnumerateInstanceVersion) check(vk.EnumerateInstanceVersion(&loaderVersion), "vkEnumerateInstanceVersion");
    // Synthetic drawing uses Vulkan 1.0. 1.1 enables core identity queries when
    // available; neither 1.2 nor 1.3 is required. Codec minimum is Stage 3 work.
    application.apiVersion = !options.api10 && loaderVersion >= VK_API_VERSION_1_1 ? VK_API_VERSION_1_1 : VK_API_VERSION_1_0;
    if (options.deviceOnly || options.minimumApi > VK_API_VERSION_1_1) {
        if (loaderVersion < options.minimumApi) throw std::runtime_error("no suitable Stage 3 loader API");
        application.apiVersion = options.minimumApi;
    }
    application.pApplicationName = "Asteria isolated Stage 2 Vulkan probe";
    if (options.deviceOnly) application.pApplicationName = "Asteria offline Stage 3 shared-device proof";
#ifdef PYROWAVE_VULKAN_STAGE4
    if (options.video) application.pApplicationName = "Asteria offline Stage 4 native video proof";
#endif
    application.applicationVersion = 1;
    log("loader_api=" + version(loaderVersion) + " requested_instance_api=" + version(application.apiVersion) +
        (options.deviceOnly ? " offline_core_policy=1.2_plus_extensions" : " synthetic_core_min=1.0"));
    uint32_t count = 0;
    check(vk.EnumerateInstanceExtensionProperties(nullptr, &count, nullptr), "instance extension count");
    std::vector<VkExtensionProperties> available(count);
    check(vk.EnumerateInstanceExtensionProperties(nullptr, &count, available.data()), "instance extensions");
    unsigned sdlCount = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(window, &sdlCount, nullptr)) throw std::runtime_error(SDL_GetError());
    instanceExtensions.resize(sdlCount);
    if (!SDL_Vulkan_GetInstanceExtensions(window, &sdlCount, instanceExtensions.data())) throw std::runtime_error(SDL_GetError());
    for (auto name : instanceExtensions) if (!has(available, name)) throw std::runtime_error(std::string("missing surface extension ") + name);
    // Optional 1.0 diagnostic property queries, not new presentation features.
    if (application.apiVersion < VK_API_VERSION_1_1 && has(available, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
        instanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    check(vk.EnumerateInstanceLayerProperties(&count, nullptr), "layer count");
    std::vector<VkLayerProperties> availableLayers(count);
    check(vk.EnumerateInstanceLayerProperties(&count, availableLayers.data()), "layers");
    const bool layer = std::any_of(availableLayers.begin(), availableLayers.end(), [](const auto& p) {
        return !std::strcmp(p.layerName, "VK_LAYER_KHRONOS_validation");
    });
    validationActive = options.validation && layer && has(available, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    if (validationActive) {
        layers.push_back("VK_LAYER_KHRONOS_validation");
        instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugInfo.pfnUserCallback = validation;
        debugInfo.pUserData = this;
        instanceInfo.pNext = &debugInfo;
        check(vk.EnumerateInstanceExtensionProperties(layers[0], &count, nullptr), "validation extension count");
        std::vector<VkExtensionProperties> layerExtensions(count);
        check(vk.EnumerateInstanceExtensionProperties(layers[0], &count, layerExtensions.data()), "validation extensions");
        if (has(layerExtensions, VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME) || has(available, VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME)) {
            instanceExtensions.push_back(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
            validationInfo.enabledValidationFeatureCount = 1;
            validationInfo.pEnabledValidationFeatures = &syncValidation;
            validationInfo.pNext = &debugInfo;
            instanceInfo.pNext = &validationInfo;
            log("synchronization_validation=ENABLED");
        } else log("synchronization_validation=SKIP unavailable extension");
        log("validation=ENABLED");
    } else log("validation=SKIP " + std::string(options.validation ? "layer/debug-utils unavailable" : "explicitly disabled"));
    instanceInfo.pApplicationInfo = &application;
    instanceInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
    instanceInfo.ppEnabledExtensionNames = instanceExtensions.data();
    instanceInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
    instanceInfo.ppEnabledLayerNames = layers.data();
    for (auto name : instanceExtensions) log(std::string("instance_extension=") + name);
    check(vk.CreateInstance(&instanceInfo, nullptr, &instance), "vkCreateInstance");
    vk.instance(instance);
    if (application.apiVersion < VK_API_VERSION_1_1) {
        const bool enabled = std::find(instanceExtensions.begin(), instanceExtensions.end(),
            std::string(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)) != instanceExtensions.end();
        vk.GetPhysicalDeviceProperties2 = enabled ? reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
            vk.GetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties2KHR")) : nullptr;
    }
    checkpoint("instance");
    if (validationActive) {
        if (!vk.CreateDebugUtilsMessengerEXT || !vk.DestroyDebugUtilsMessengerEXT) throw std::runtime_error("debug messenger exports missing");
        check(vk.CreateDebugUtilsMessengerEXT(instance, &debugInfo, nullptr, &messenger), "vkCreateDebugUtilsMessengerEXT");
    }
    if (!SDL_Vulkan_CreateSurface(window, instance, &surface)) throw std::runtime_error(std::string("SDL surface: ") + SDL_GetError());
    log("surface_created=YES on existing SDL window");
    checkpoint("surface");
    selectDevice();
    queueInfo.queueFamilyIndex = family;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;
    deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.pEnabledFeatures = &features; // All zero: no synthetic feature requirements.
    deviceInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();
    if (options.configure) options.configure(deviceInfo);
    check(vk.CreateDevice(physical, &deviceInfo, nullptr, &device), "vkCreateDevice");
    vk.device(device, !options.deviceOnly);
    vk.GetDeviceQueue(device, family, 0, &queue);
    log(std::string(options.deviceOnly ? "offline_device=YES" : (options.configure ? "offline_device=YES device_extension=VK_KHR_swapchain" : "device_extension=VK_KHR_swapchain features=none"))+
        " queue_family=" + std::to_string(family) + " queue_index=0");
    checkpoint("device");
    if (options.deviceOnly) return;
    VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pool.queueFamilyIndex = family;
    pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    check(vk.CreateCommandPool(device, &pool, nullptr, &commandPool), "vkCreateCommandPool");
    checkpoint("command-pool");
    for (auto& frame : frames) {
        VkCommandBufferAllocateInfo allocation{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocation.commandPool = commandPool;
        allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocation.commandBufferCount = 1;
        check(vk.AllocateCommandBuffers(device, &allocation, &frame.command), "vkAllocateCommandBuffers");
        VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        check(vk.CreateSemaphore(device, &semaphore, nullptr, &frame.acquired), "vkCreateSemaphore acquire");
        VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        check(vk.CreateFence(device, &fence, nullptr, &frame.complete), "vkCreateFence");
        fence.flags = 0;
        check(vk.CreateFence(device, &fence, nullptr, &frame.acquireComplete), "vkCreateFence acquire");
    }
    checkpoint("command-resources");
    recreate();
}
void Probe::selectDevice() {
    uint32_t count = 0;
    check(vk.EnumeratePhysicalDevices(instance, &count, nullptr), "physical device count");
    std::vector<VkPhysicalDevice> devices(count);
    check(vk.EnumeratePhysicalDevices(instance, &count, devices.data()), "physical devices");
    for (auto candidate : devices) {
        VkPhysicalDeviceProperties properties{};
        vk.GetPhysicalDeviceProperties(candidate, &properties);
        log("candidate=" + std::string(properties.deviceName) + " device_api=" + version(properties.apiVersion) +
            " vendor_id=" + std::to_string(properties.vendorID) + " device_id=" + std::to_string(properties.deviceID) +
            " driver_version_raw=" + std::to_string(properties.driverVersion));
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) continue;
        if (properties.apiVersion < options.minimumApi) continue;
        if (options.suitable && !options.suitable(candidate)) continue;
        check(vk.EnumerateDeviceExtensionProperties(candidate, nullptr, &count, nullptr), "device extension count");
        std::vector<VkExtensionProperties> extensions(count);
        check(vk.EnumerateDeviceExtensionProperties(candidate, nullptr, &count, extensions.data()), "device extensions");
        log("present_fence_extension_ext=" + std::to_string(has(extensions, VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME)) +
            " khr=" + std::to_string(has(extensions, VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME)));
        if (!has(extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) continue;
        vk.GetPhysicalDeviceQueueFamilyProperties(candidate, &count, nullptr);
        std::vector<VkQueueFamilyProperties> families(count);
        vk.GetPhysicalDeviceQueueFamilyProperties(candidate, &count, families.data());
        std::vector<QueueCandidate> policies;
        for (uint32_t i = 0; i < count; ++i) {
            VkBool32 present = VK_FALSE;
            check(vk.GetPhysicalDeviceSurfaceSupportKHR(candidate, i, surface, &present), "surface support");
            policies.push_back({bool(families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT), bool(families[i].queueFlags & VK_QUEUE_COMPUTE_BIT), bool(present), families[i].queueCount});
        }
        const int selected = queueFamily(policies);
        if (selected < 0) continue;
        physical = candidate;
        family = static_cast<uint32_t>(selected);
        selectedDevice = properties.deviceName;
        log("selected_device=" + selectedDevice + " device_api=" + version(properties.apiVersion));
        // Expected name is an owner assertion, never an adapter identity matcher.
        if (!options.deviceName.empty() && selectedDevice.find(options.deviceName) == std::string::npos)
            throw std::runtime_error("selected device does not satisfy --expect-device assertion");
        if (vk.GetPhysicalDeviceProperties2) {
            VkPhysicalDeviceProperties2 p{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
            VkPhysicalDeviceIDProperties ids{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};
            VkPhysicalDeviceDriverProperties driver{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
            const bool idAvailable = application.apiVersion >= VK_API_VERSION_1_1 && properties.apiVersion >= VK_API_VERSION_1_1;
            const bool driverAvailable = properties.apiVersion >= VK_API_VERSION_1_2 || has(extensions, VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME);
            if (idAvailable) p.pNext = &ids;
            if (driverAvailable) { driver.pNext = p.pNext; p.pNext = &driver; }
            vk.GetPhysicalDeviceProperties2(candidate, &p);
            if (idAvailable) log("device_uuid=" + hexBytes(ids.deviceUUID, VK_UUID_SIZE) + " driver_uuid=" + hexBytes(ids.driverUUID, VK_UUID_SIZE) +
                " luid_valid=" + std::to_string(ids.deviceLUIDValid) + " luid=" + hexBytes(ids.deviceLUID, VK_LUID_SIZE) + " node_mask=" + std::to_string(ids.deviceNodeMask));
            else log("uuid_luid=SKIP Vulkan 1.1 identity query unavailable");
            if (driverAvailable) log("driver_id=" + std::to_string(driver.driverID) + " driver_name=" + driver.driverName + " driver_info=" + driver.driverInfo);
            else log("driver_properties=SKIP unavailable");
        } else log("extended_identity=SKIP properties2 unavailable");
        if (!options.deviceOnly && !options.configure) log("external_handles=NONE pyrowave_runtime=NOT_LOADED");
        return;
    }
    throw std::runtime_error("no non-software physical device with swapchain and graphics+compute+present queue");
}
void Probe::destroySwapchain() noexcept {
    for (auto& image : images) {
        if (image.framebuffer) vk.DestroyFramebuffer(device, image.framebuffer, nullptr);
        if (image.view) vk.DestroyImageView(device, image.view, nullptr);
        if (image.presented) vk.DestroySemaphore(device, image.presented, nullptr);
    }
    images.clear();
    if (renderPass) vk.DestroyRenderPass(device, renderPass, nullptr);
    renderPass = VK_NULL_HANDLE;
    if (swapchain) vk.DestroySwapchainKHR(device, swapchain, nullptr);
    swapchain = VK_NULL_HANDLE;
}
bool Probe::recreate() {
    mainThread();
    int width = 0, height = 0;
    SDL_Vulkan_GetDrawableSize(window, &width, &height);
    if (width <= 0 || height <= 0 || (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)) return false;
    // Rare resize/teardown drain only, never in the per-frame submission path.
    if (swapchain) check(vk.DeviceWaitIdle(device), "resize vkDeviceWaitIdle");
    destroySwapchain();
    VkSurfaceCapabilitiesKHR capabilities{};
    check(vk.GetPhysicalDeviceSurfaceCapabilitiesKHR(physical, surface, &capabilities), "surface capabilities");
    auto selectedExtent = chooseExtent({capabilities.currentExtent.width, capabilities.currentExtent.height},
        {static_cast<uint32_t>(width), static_cast<uint32_t>(height)},
        {capabilities.minImageExtent.width, capabilities.minImageExtent.height}, {capabilities.maxImageExtent.width, capabilities.maxImageExtent.height});
    if (!selectedExtent.width || !selectedExtent.height) return false;
    extent = {selectedExtent.width, selectedExtent.height};
    if (!(capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)) throw std::runtime_error("surface does not support color attachments");
    uint32_t count = 0;
    check(vk.GetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &count, nullptr), "format count");
    std::vector<VkSurfaceFormatKHR> formats(count);
    check(vk.GetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &count, formats.data()), "surface formats");
    VkSurfaceFormatKHR format{VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    for (auto preferred : {VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM})
        for (auto f : formats) if ((f.format == preferred || f.format == VK_FORMAT_UNDEFINED) && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR && format.format == VK_FORMAT_UNDEFINED)
            format = {preferred, f.colorSpace};
    if (format.format == VK_FORMAT_UNDEFINED) throw std::runtime_error("no SDR UNORM surface format");
    swapchainFormat = format.format;
    check(vk.GetPhysicalDeviceSurfacePresentModesKHR(physical, surface, &count, nullptr), "mode count");
    std::vector<VkPresentModeKHR> modes(count);
    check(vk.GetPhysicalDeviceSurfacePresentModesKHR(physical, surface, &count, modes.data()), "present modes");
    std::vector<Mode> modePolicies;
    for (auto m : modes) {
        if (m == VK_PRESENT_MODE_IMMEDIATE_KHR) modePolicies.push_back(Mode::Immediate);
        if (m == VK_PRESENT_MODE_FIFO_RELAXED_KHR) modePolicies.push_back(Mode::Relaxed);
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) modePolicies.push_back(Mode::Mailbox);
        if (m == VK_PRESENT_MODE_FIFO_KHR) modePolicies.push_back(Mode::Fifo);
    }
    const auto mode = nativeMode(presentMode(options.vsync, modePolicies));
    VkSwapchainCreateInfoKHR info{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    info.surface = surface;
    info.minImageCount = imageCount(capabilities.minImageCount, capabilities.maxImageCount);
    info.imageFormat = format.format;
    info.imageColorSpace = format.colorSpace;
    info.imageExtent = extent;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.preTransform = capabilities.currentTransform;
    for (auto alpha : {VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR})
        if (capabilities.supportedCompositeAlpha & alpha) { info.compositeAlpha = alpha; break; }
    info.presentMode = mode;
    info.clipped = VK_TRUE;
    check(vk.CreateSwapchainKHR(device, &info, nullptr, &swapchain), "vkCreateSwapchainKHR");
    checkpoint("swapchain");
    check(vk.GetSwapchainImagesKHR(device, swapchain, &count, nullptr), "swapchain image count");
    std::vector<VkImage> handles(count);
    check(vk.GetSwapchainImagesKHR(device, swapchain, &count, handles.data()), "swapchain images");
    images.resize(count);
    VkAttachmentDescription attachment{};
    attachment.format = format.format;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentReference reference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &reference;
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    VkRenderPassCreateInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    pass.attachmentCount = 1; pass.pAttachments = &attachment;
    pass.subpassCount = 1; pass.pSubpasses = &subpass;
    pass.dependencyCount = 1; pass.pDependencies = &dependency;
    check(vk.CreateRenderPass(device, &pass, nullptr, &renderPass), "vkCreateRenderPass");
    checkpoint("render-pass");
    for (size_t i = 0; i < images.size(); ++i) {
        auto& image = images[i];
        image.image = handles[i];
        VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view.image = image.image; view.viewType = VK_IMAGE_VIEW_TYPE_2D; view.format = format.format;
        view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        check(vk.CreateImageView(device, &view, nullptr, &image.view), "vkCreateImageView");
        checkpoint("image-view");
        VkFramebufferCreateInfo framebuffer{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        framebuffer.renderPass = renderPass; framebuffer.attachmentCount = 1; framebuffer.pAttachments = &image.view;
        framebuffer.width = extent.width; framebuffer.height = extent.height; framebuffer.layers = 1;
        check(vk.CreateFramebuffer(device, &framebuffer, nullptr, &image.framebuffer), "vkCreateFramebuffer");
        checkpoint("framebuffer");
        VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        check(vk.CreateSemaphore(device, &semaphore, nullptr, &image.presented), "vkCreateSemaphore present");
        checkpoint("present-semaphore");
    }
    progress.rebuild = false;
    ++recreations;
    log("swapchain_generation=" + std::to_string(recreations) + " extent=" + std::to_string(extent.width) + "x" + std::to_string(extent.height) +
        " images=" + std::to_string(images.size()) + " present_mode=" + std::to_string(mode) + " format=" + std::to_string(format.format));
    return true;
}
void Probe::record(VkCommandBuffer command, uint32_t index) {
    check(vk.ResetCommandBuffer(command, 0), "vkResetCommandBuffer");
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    check(vk.BeginCommandBuffer(command, &begin), "vkBeginCommandBuffer");
    VkClearValue background{};
    background.color.float32[0] = background.color.float32[1] = background.color.float32[2] = 0.15f;
    background.color.float32[3] = 1.0f;
#ifdef PYROWAVE_VULKAN_STAGE4
    if (options.video) {
        background.color.float32[0] = background.color.float32[1] = background.color.float32[2] = 0;
        if (options.beforeVideo) options.beforeVideo(command);
    }
#endif
    VkRenderPassBeginInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    pass.renderPass = renderPass; pass.framebuffer = images[index].framebuffer; pass.renderArea.extent = extent;
    pass.clearValueCount = 1; pass.pClearValues = &background;
    vk.CmdBeginRenderPass(command, &pass, VK_SUBPASS_CONTENTS_INLINE);
#ifdef PYROWAVE_VULKAN_STAGE4
    if (options.video) {
        options.video(command, renderPass, extent);
        vk.CmdEndRenderPass(command);
        check(vk.EndCommandBuffer(command), "vkEndCommandBuffer video");
        return;
    }
#endif
    const float colors[6][4] = {{1,0,0,1},{0,1,0,1},{0,0,1,1},{0,1,1,1},{1,0,1,1},{1,1,0,1}};
    for (uint32_t i = 0; i < 6; ++i) {
        const uint32_t left = extent.width * i / 6, right = extent.width * (i + 1) / 6;
        if (right == left) continue;
        VkClearAttachment clear{}; clear.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        std::copy(colors[i], colors[i] + 4, clear.clearValue.color.float32);
        VkClearRect rectangle{{{static_cast<int32_t>(left), 0}, {right - left, extent.height * 3 / 4}}, 0, 1};
        if (rectangle.rect.extent.height) vk.CmdClearAttachments(command, 1, &clear, 1, &rectangle);
    }
    for (uint32_t i = 0; i < 3; ++i) {
        const uint32_t left = extent.width * i / 3, right = extent.width * (i + 1) / 3;
        if (right == left) continue;
        VkClearAttachment clear{}; clear.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        const float level = i == 0 ? 0.0f : (i == 1 ? 1.0f : 0.5f);
        clear.clearValue.color = {{level, level, level, 1.0f}};
        VkClearRect rectangle{{{static_cast<int32_t>(left), static_cast<int32_t>(extent.height * 3 / 4)}, {right - left, extent.height - extent.height * 3 / 4}}, 0, 1};
        vk.CmdClearAttachments(command, 1, &clear, 1, &rectangle);
    }
    vk.CmdEndRenderPass(command);
    check(vk.EndCommandBuffer(command), "vkEndCommandBuffer");
}
bool Probe::draw() {
    mainThread();
    int width = 0, height = 0;
    SDL_Vulkan_GetDrawableSize(window, &width, &height);
    if (width <= 0 || height <= 0 || (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)) return false;
    if (progress.rebuild || !swapchain) { recreate(); return false; }
    auto& frame = frames[progress.frameResource];
    const VkResult complete = vk.GetFenceStatus(device, frame.complete);
    if (complete == VK_NOT_READY) return false; // One query per SDL wait iteration.
    check(complete, "vkGetFenceStatus");
    if (frame.acquisitionPending) {
        const VkResult acquisition = vk.GetFenceStatus(device, frame.acquireComplete);
        if (acquisition == VK_NOT_READY) return false;
        check(acquisition, "vkGetFenceStatus acquire");
    }
    // Check both completion domains explicitly before resetting the acquisition
    // fence. It also protects partial-submit cleanup and is never host-waited
    // in the presentation hot path.
    check(vk.ResetFences(device, 1, &frame.acquireComplete), "vkResetFences acquire");
    frame.acquisitionPending = false;
    uint32_t index = 0;
    // Finite acquire, never UINT64_MAX. NOT_READY/TIMEOUT leave semaphore unused.
    const VkResult acquired = vk.AcquireNextImageKHR(device, swapchain, 1000000, frame.acquired, frame.acquireComplete, &index);
    frame.acquisitionPending = acquired == VK_SUCCESS || acquired == VK_SUBOPTIMAL_KHR;
    Acquire outcome = Acquire::Failed;
    if (acquired == VK_SUCCESS) outcome = Acquire::Ready;
    if (acquired == VK_SUBOPTIMAL_KHR) outcome = Acquire::Suboptimal;
    if (acquired == VK_NOT_READY || acquired == VK_TIMEOUT) outcome = Acquire::NotReady;
    if (acquired == VK_ERROR_OUT_OF_DATE_KHR) outcome = Acquire::OutOfDate;
    const auto action = progress.decide({static_cast<uint32_t>(width), static_cast<uint32_t>(height)}, true, outcome);
    if (action == Action::Retry || action == Action::Suspend) return false;
    if (action == Action::Rebuild) { recreate(); return false; }
    if (action == Action::Stop) check(acquired, "vkAcquireNextImageKHR");
    if (index >= images.size()) throw std::runtime_error("invalid acquired image index");
    record(frame.command, index);
    check(vk.ResetFences(device, 1, &frame.complete), "vkResetFences");
    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.waitSemaphoreCount = 1; submit.pWaitSemaphores = &frame.acquired; submit.pWaitDstStageMask = &waitStage;
    submit.commandBufferCount = 1; submit.pCommandBuffers = &frame.command;
    submit.signalSemaphoreCount = 1; submit.pSignalSemaphores = &images[index].presented;
#ifdef PYROWAVE_VULKAN_STAGE4
    if (options.videoSubmit) options.videoSubmit(submit);
    const auto submitStarted = std::chrono::steady_clock::now();
#endif
    check(vk.QueueSubmit(queue, 1, &submit, frame.complete), "vkQueueSubmit");
#ifdef PYROWAVE_VULKAN_STAGE4
    queueSubmitMs += std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-submitStarted).count();
    if (options.videoSubmitted) options.videoSubmitted();
#endif
    VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present.waitSemaphoreCount = 1; present.pWaitSemaphores = &images[index].presented;
    present.swapchainCount = 1; present.pSwapchains = &swapchain; present.pImageIndices = &index;
#ifdef PYROWAVE_VULKAN_STAGE4
    const auto presentStarted = std::chrono::steady_clock::now();
#endif
    const VkResult result = vk.QueuePresentKHR(queue, &present);
#ifdef PYROWAVE_VULKAN_STAGE4
    presentCallMs += std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-presentStarted).count();
#endif
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) progress.rebuild = true;
    else check(result, "vkQueuePresentKHR");
    progress.submitted();
    if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) ++presents;
    return true;
}
void Probe::close() noexcept {
    // Main-thread-only owner; close is idempotent, including partial initialize.
    if (device) {
        if (vk.DeviceWaitIdle) {
            const auto result = vk.DeviceWaitIdle(device);
            log("teardown_device_drain=" + std::to_string(result));
            cleanupOkay = cleanupOkay && result == VK_SUCCESS;
        }
        for (auto& frame : frames) if (frame.acquisitionPending) {
            const auto result = vk.WaitForFences(device, 1, &frame.acquireComplete, VK_TRUE, 5000000000ull);
            log("teardown_acquisition_completion=" + std::to_string(result));
            cleanupOkay = cleanupOkay && result == VK_SUCCESS;
            frame.acquisitionPending = false;
        }
        destroySwapchain();
        for (auto& frame : frames) {
            if (frame.acquired) vk.DestroySemaphore(device, frame.acquired, nullptr);
            if (frame.complete) vk.DestroyFence(device, frame.complete, nullptr);
            if (frame.acquireComplete) vk.DestroyFence(device, frame.acquireComplete, nullptr);
            frame = {};
        }
        if (commandPool) vk.DestroyCommandPool(device, commandPool, nullptr);
        commandPool = VK_NULL_HANDLE;
        vk.DestroyDevice(device, nullptr);
        device = VK_NULL_HANDLE;
    }
    if (surface) { vk.DestroySurfaceKHR(instance, surface, nullptr); surface = VK_NULL_HANDLE; }
    if (messenger) { vk.DestroyDebugUtilsMessengerEXT(instance, messenger, nullptr); messenger = VK_NULL_HANDLE; }
    if (instance) { vk.DestroyInstance(instance, nullptr); instance = VK_NULL_HANDLE; }
}
} // namespace PyroWaveVulkan
