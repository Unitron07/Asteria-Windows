#include "pyrowave_vulkan_dispatch.h"
#include <vector>
namespace PyroWaveVulkan {
Dispatch::~Dispatch() { if (module) FreeLibrary(module); }
PFN_vkVoidFunction Dispatch::required(PFN_vkVoidFunction value, const char* name) const {
    if (!value || missingExport == name) throw std::runtime_error(std::string("missing Vulkan export: ") + name);
    return value;
}
void Dispatch::load() {
    wchar_t directory[MAX_PATH];
    const UINT length = GetSystemDirectoryW(directory, MAX_PATH);
    if (!length || length >= MAX_PATH) throw std::runtime_error("GetSystemDirectoryW failed");
    std::wstring path(directory, length);
    path += L"\\vulkan-1.dll";
    module = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!module) throw std::runtime_error("System32 Vulkan loader unavailable, Win32=" + std::to_string(GetLastError()));
    char utf8[MAX_PATH * 4];
    if (!WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, utf8, sizeof(utf8), nullptr, nullptr))
        throw std::runtime_error("Vulkan loader path conversion failed");
    loaderPath = utf8;
    GetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(required(
        reinterpret_cast<PFN_vkVoidFunction>(GetProcAddress(module, "vkGetInstanceProcAddr")), "vkGetInstanceProcAddr"));
#define PW_LOAD(name) name = reinterpret_cast<PFN_vk##name>(required(GetInstanceProcAddr(VK_NULL_HANDLE, "vk" #name), "vk" #name));
    PW_GLOBAL(PW_LOAD)
#undef PW_LOAD
    EnumerateInstanceVersion = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(GetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"));
}
void Dispatch::instance(VkInstance instance) {
#define PW_LOAD(name) name = reinterpret_cast<PFN_vk##name>(required(GetInstanceProcAddr(instance, "vk" #name), "vk" #name));
    PW_INSTANCE(PW_LOAD)
#undef PW_LOAD
    GetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(required(GetInstanceProcAddr(instance, "vkGetDeviceProcAddr"), "vkGetDeviceProcAddr"));
    GetPhysicalDeviceProperties2 = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(GetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties2"));
    if (!GetPhysicalDeviceProperties2)
        GetPhysicalDeviceProperties2 = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(GetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties2KHR"));
    CreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(GetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
    DestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(GetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
}
void Dispatch::device(VkDevice device) {
#define PW_LOAD(name) name = reinterpret_cast<PFN_vk##name>(required(GetDeviceProcAddr(device, "vk" #name), "vk" #name));
    PW_DEVICE(PW_LOAD)
#undef PW_LOAD
}
} // namespace PyroWaveVulkan
