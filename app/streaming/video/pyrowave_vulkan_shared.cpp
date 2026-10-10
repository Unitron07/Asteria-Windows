#include "pyrowave_vulkan_shared.h"
#include <algorithm>
#include <cstring>
using PyroWaveVulkan::check;
namespace Stage3 {
template<class T> void resolve(T& target,PFN_vkVoidFunction value,const char* name) {
    if (!value) throw std::runtime_error(std::string("missing Stage 3 Vulkan export: ")+name);
    target=reinterpret_cast<T>(value);
}
void NativeDispatch::loadInstance(PyroWaveVulkan::Dispatch& vk,VkInstance instance) {
#define LOAD(name) resolve(name,vk.GetInstanceProcAddr(instance,"vk" #name),"vk" #name);
    STAGE3_INSTANCE(LOAD)
#undef LOAD
}
void NativeDispatch::loadDevice(PyroWaveVulkan::Dispatch& vk,VkDevice device) {
#define LOAD(name) resolve(name,vk.GetDeviceProcAddr(device,"vk" #name),"vk" #name);
    STAGE3_DEVICE(LOAD)
#undef LOAD
}
bool Requirements::suitable(VkPhysicalDevice physical) {
    // Called after the owner's instance exists; use core 1.2 plus reviewed extensions.
    VkPhysicalDeviceProperties properties{}; vk.GetPhysicalDeviceProperties(physical,&properties);
    uint32_t count=0;
    check(vk.EnumerateDeviceExtensionProperties(physical,nullptr,&count,nullptr),"extension count");
    std::vector<VkExtensionProperties> available(count);
    check(vk.EnumerateDeviceExtensionProperties(physical,nullptr,&count,available.data()),"extensions");
    for (auto name:extensions) if (std::none_of(available.begin(),available.end(),[&](const auto& e) { return !std::strcmp(e.extensionName,name); })) {
        log(std::string("candidate_missing_extension=")+name); return false;
    }
    features.pNext=&f11; f11.pNext=&f12; f12.pNext=&subgroup; subgroup.pNext=&sync2;
    native.GetPhysicalDeviceFeatures2(physical,&features);
    VkPhysicalDeviceProperties2 p{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    VkPhysicalDeviceSubgroupProperties ops{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
    VkPhysicalDeviceSubgroupSizeControlProperties sizes{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES};
    p.pNext=&ops; ops.pNext=&sizes; vk.GetPhysicalDeviceProperties2(physical,&p);
    constexpr VkSubgroupFeatureFlags required=VK_SUBGROUP_FEATURE_BASIC_BIT | VK_SUBGROUP_FEATURE_VOTE_BIT |
        VK_SUBGROUP_FEATURE_BALLOT_BIT | VK_SUBGROUP_FEATURE_ARITHMETIC_BIT | VK_SUBGROUP_FEATURE_SHUFFLE_BIT | VK_SUBGROUP_FEATURE_SHUFFLE_RELATIVE_BIT;
    // Match Granite supports_subgroup_size_log2(true,2,7), not a vendor table.
    bool range=(sizes.minSubgroupSize>=4 && sizes.maxSubgroupSize<=128) ||
        (sizes.maxSubgroupSize>=4 && sizes.minSubgroupSize<=128 && (sizes.requiredSubgroupSizeStages & VK_SHADER_STAGE_COMPUTE_BIT));
    const bool texel=properties.limits.maxTexelBufferElements>=16u*1024*1024;
    if (texel) for (auto format:{VK_FORMAT_R8_UINT,VK_FORMAT_R16_UINT,VK_FORMAT_R32_UINT}) {
        VkFormatProperties f{}; native.GetPhysicalDeviceFormatProperties(physical,format,&f);
        if (!(f.bufferFeatures & VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT)) {
            log("candidate_missing_uniform_texel_format="+std::to_string(format)); return false;
        }
    }
    for (auto format:{VK_FORMAT_R16_SFLOAT,VK_FORMAT_R32_SFLOAT}) {
        VkFormatProperties f{}; native.GetPhysicalDeviceFormatProperties(physical,format,&f);
        const auto required=VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
        if ((f.optimalTilingFeatures & required)!=required) {
            log("candidate_missing_wavelet_image_format="+std::to_string(format)); return false;
        }
    }
    const bool storage=f12.storageBuffer8BitAccess && f11.storageBuffer16BitAccess;
    Capabilities c{properties.apiVersion>=VK_API_VERSION_1_2,bool(f12.timelineSemaphore),bool(sync2.synchronization2),
        bool(subgroup.subgroupSizeControl),bool(subgroup.computeFullSubgroups),range,
        (ops.supportedOperations & required)==required && bool(ops.supportedStages & VK_SHADER_STAGE_COMPUTE_BIT),
        storage,texel,bool(features.features.shaderStorageImageWriteWithoutFormat)};
    log("candidate_stage3_requirements="+std::string(supported(c) ? "YES" : "NO")+
        " subgroup_operations="+std::to_string(ops.supportedOperations)+" subgroup_min="+std::to_string(sizes.minSubgroupSize)+
        " subgroup_max="+std::to_string(sizes.maxSubgroupSize)+" texel_fallback="+std::to_string(texel));
    if (!supported(c)) return false;
    // Prefer the codec's texel fallback when it can work. No optional float16/int16 arithmetic.
    features.features={}; features.features.shaderStorageImageWriteWithoutFormat=VK_TRUE;
    f11={VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
    f12={VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    f12.timelineSemaphore=VK_TRUE;
    if (!texel) { f11.storageBuffer16BitAccess=VK_TRUE; f12.storageBuffer8BitAccess=VK_TRUE; }
    features.pNext=&f11; f11.pNext=&f12; f12.pNext=&subgroup; subgroup.pNext=&sync2;
    return true;
}
void Requirements::configure(VkDeviceCreateInfo& info) {
    info.pNext=&features; info.pEnabledFeatures=nullptr;
    // The surface is retained for queue compatibility, but no swapchain is created.
    info.enabledExtensionCount=uint32_t(extensions.size()); info.ppEnabledExtensionNames=extensions.data();
    log("enabled_features=timelineSemaphore,subgroupSizeControl,computeFullSubgroups,synchronization2,shaderStorageImageWriteWithoutFormat"+
        std::string(f12.storageBuffer8BitAccess ? ",storageBuffer8BitAccess,storageBuffer16BitAccess" : ""));
    for (auto name:extensions) log(std::string("enabled_device_extension=")+name);
    log("shaderFloat16=DISABLED optional shaderInt16=DISABLED encoder_only stage3_api_policy=1.2_plus_extensions");
}
}
