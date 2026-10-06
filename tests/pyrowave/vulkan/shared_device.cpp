#include "shared_device.h"
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
Outputs::Outputs(PyroWaveVulkan::Probe& o,PyroWaveVulkan::Dispatch& v,NativeDispatch& n,QueueLock& q,PyroWaveVulkan::Log l,Path p)
    :owner(o),vk(v),native(n),queueLock(q),log(std::move(l)),path(p) {}
Outputs::~Outputs() { close(); }
void Outputs::allocate(Resource& r,VkMemoryRequirements req,VkMemoryPropertyFlags required,VkMemoryPropertyFlags preferred) {
    int type=-1;
    for (unsigned i=0;i<memory.memoryTypeCount;++i) if ((req.memoryTypeBits & (1u<<i)) &&
        (memory.memoryTypes[i].propertyFlags & required)==required) {
        if (type<0) type=int(i);
        if ((memory.memoryTypes[i].propertyFlags & preferred)==preferred) { type=int(i); break; }
    }
    if (type<0) throw std::runtime_error("no compatible memory type");
    r.memoryType=uint32_t(type); r.coherent=(memory.memoryTypes[type].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)!=0;
    VkMemoryAllocateInfo info{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; info.allocationSize=req.size; info.memoryTypeIndex=r.memoryType;
    check(native.AllocateMemory(owner.deviceHandle(),&info,nullptr,&r.allocation),"allocate caller memory");
}
void Outputs::begin() {
    check(vk.ResetCommandBuffer(command,0),"reset readback command");
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    check(vk.BeginCommandBuffer(command,&begin),"begin caller command");
}
void Outputs::submit(VkSemaphore timeline,uint64_t waitValue,uint64_t signal) {
    if (!signal || (waitValue && waitValue>=signal)) throw std::runtime_error("invalid caller timeline payload");
    check(vk.EndCommandBuffer(command),"end caller command");
    VkTimelineSemaphoreSubmitInfo values{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
    values.waitSemaphoreValueCount=waitValue ? 1u : 0u; values.pWaitSemaphoreValues=&waitValue;
    values.signalSemaphoreValueCount=1; values.pSignalSemaphoreValues=&signal;
    VkPipelineStageFlags stage=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.pNext=&values;
    submit.waitSemaphoreCount=waitValue ? 1u : 0u; submit.pWaitSemaphores=&timeline; submit.pWaitDstStageMask=&stage;
    submit.signalSemaphoreCount=1; submit.pSignalSemaphores=&timeline;
    submit.commandBufferCount=1; submit.pCommandBuffers=&command;
    { QueueLock::Guard guard(queueLock); check(vk.QueueSubmit(owner.queueHandle(),1,&submit,VK_NULL_HANDLE),"caller queue submit"); }
}
void Outputs::wait(VkSemaphore timeline,uint64_t value) {
    if (!value) throw std::runtime_error("zero timeline host wait");
    VkSemaphoreWaitInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO}; info.semaphoreCount=1; info.pSemaphores=&timeline; info.pValues=&value;
    check(native.WaitSemaphores(owner.deviceHandle(),&info,30000000000ull),"offline timeline wait");
}
void Outputs::initialize(const std::string& fault) {
    auto device=owner.deviceHandle(); auto physical=owner.physicalHandle();
    native.GetPhysicalDeviceMemoryProperties(physical,&memory);
    // Internal codec formats are required independently of output path. Query before codec allocation.
    for (auto format:{VK_FORMAT_R16_SFLOAT,VK_FORMAT_R32_SFLOAT}) {
        VkFormatProperties props{}; native.GetPhysicalDeviceFormatProperties(physical,format,&props);
        auto required=VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
        if ((props.optimalTilingFeatures & required)!=required) throw std::runtime_error("unsupported internal wavelet format");
    }
    if (path==Path::Fragment) for (auto format:{VK_FORMAT_R16_SFLOAT,VK_FORMAT_R16G16_SFLOAT}) {
        VkFormatProperties props{}; native.GetPhysicalDeviceFormatProperties(physical,format,&props);
        auto required=VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
        if ((props.optimalTilingFeatures & required)!=required) throw std::runtime_error("unsupported internal fragment format");
    }
    VkImageFormatProperties support{};
    check(native.GetPhysicalDeviceImageFormatProperties(physical,VK_FORMAT_R8_UNORM,VK_IMAGE_TYPE_2D,
        VK_IMAGE_TILING_OPTIMAL,usage(path),0,&support),"R8 output image-format usage query");
    if (support.maxExtent.width<1920 || support.maxExtent.height<1080 || !(support.sampleCounts & VK_SAMPLE_COUNT_1_BIT))
        throw std::runtime_error("unsupported caller plane extent/sample count");
    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.queueFamilyIndex=owner.queueFamilyIndex(); poolInfo.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    check(vk.CreateCommandPool(device,&poolInfo,nullptr,&pool),"caller command pool");
    VkCommandBufferAllocateInfo alloc{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    alloc.commandPool=pool; alloc.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; alloc.commandBufferCount=1;
    check(vk.AllocateCommandBuffers(device,&alloc,&command),"caller command buffer");
    const auto metadata=planes(1920,1080);
    for (unsigned index=0;index<SlotCount;++index) {
        auto& slot=slots[index];
        VkSemaphoreTypeCreateInfo type{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO}; type.semaphoreType=VK_SEMAPHORE_TYPE_TIMELINE;
        VkSemaphoreCreateInfo sem{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO}; sem.pNext=&type;
        check(vk.CreateSemaphore(device,&sem,nullptr,&slot.timeline),"non-exportable native timeline");
        for (unsigned p=0;p<3;++p) {
            auto& image=slot.images[p]; auto& staging=slot.staging[p]; const auto plane=metadata[p];
            VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO}; info.imageType=VK_IMAGE_TYPE_2D;
            info.format=VK_FORMAT_R8_UNORM; info.extent={plane.width,plane.height,1};
            info.mipLevels=1; info.arrayLayers=1; info.samples=VK_SAMPLE_COUNT_1_BIT;
            info.tiling=VK_IMAGE_TILING_OPTIMAL; info.usage=usage(path); info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
            check(native.CreateImage(device,&info,nullptr,&image.image),"caller-owned R8 image");
            VkMemoryRequirements req{}; native.GetImageMemoryRequirements(device,image.image,&req);
            allocate(image,req,0,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            check(native.BindImageMemory(device,image.image,image.allocation,0),"bind caller image");
            if (fault=="partial-images" && index==0 && p==0) throw std::runtime_error("injected partial-images");
            VkBufferCreateInfo b{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO}; b.size=plane.bytes;
            b.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT; b.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
            check(native.CreateBuffer(device,&b,nullptr,&staging.buffer),"test-only staging buffer");
            native.GetBufferMemoryRequirements(device,staging.buffer,&req);
            allocate(staging,req,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
            check(native.BindBufferMemory(device,staging.buffer,staging.allocation,0),"bind staging buffer");
            log("plane_slot="+std::to_string(index)+" plane="+std::to_string(p)+" format=R8_UNORM width="+std::to_string(plane.width)+
                " height="+std::to_string(plane.height)+" usage="+std::to_string(info.usage)+" memory_type="+std::to_string(image.memoryType)+
                " memory_flags="+std::to_string(memory.memoryTypes[image.memoryType].propertyFlags));
        }
        begin();
        std::array<VkImageMemoryBarrier,3> barriers{};
        for (unsigned p=0;p<3;++p) {
            auto& b=barriers[p]; b.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            b.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED; b.newLayout=VK_IMAGE_LAYOUT_GENERAL;
            b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
            b.dstAccessMask=VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            b.image=slot.images[p].image; b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        }
        native.CmdPipelineBarrier(command,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,0,0,nullptr,0,nullptr,3,barriers.data());
        submit(slot.timeline,0,1); wait(slot.timeline,1); slot.initialized=true;
    }
    log("native_caller_owned_images=YES external_memory_handles=0 external_semaphore_handles=0 d3d11_resources=0");
}
pyrowave_gpu_buffers Outputs::views(unsigned index) const {
    pyrowave_gpu_buffers views{}; const auto metadata=planes(1920,1080);
    for (unsigned p=0;p<3;++p) {
        auto& v=views.planes[p]; v.image=slots.at(index).images[p].image;
        v.width=metadata[p].width; v.height=metadata[p].height;
        v.image_format=v.view_format=VK_FORMAT_R8_UNORM;
        v.aspect=VK_IMAGE_ASPECT_COLOR_BIT; v.swizzle=VK_COMPONENT_SWIZZLE_IDENTITY; v.layout=VK_IMAGE_LAYOUT_GENERAL;
    }
    return views;
}
pyrowave_gpu_sync_operation Outputs::acquire(unsigned index) const {
    pyrowave_gpu_sync_operation op{}; const auto& slot=slots.at(index); op.sync={slot.timeline,slot.payload.next()[0]}; return op;
}
pyrowave_gpu_sync_operation Outputs::release(unsigned index) const {
    pyrowave_gpu_sync_operation op{}; const auto& slot=slots.at(index); op.sync={slot.timeline,slot.payload.next()[1]}; return op;
}
void Outputs::submitted(unsigned index) { auto& s=slots.at(index); s.payload.submitted(s.payload.next()[1]); }
PyroWave::Pixels Outputs::read(unsigned index) {
    auto& slot=slots.at(index); if (!slot.payload.pending) throw std::runtime_error("read without decode submission");
    const uint64_t decode=slot.payload.decoded, consumer=decode+1;
    begin();
    // The timeline wait supplies execution and memory dependencies from codec writes.
    // Keep GENERAL; explicit barriers identify the following copy and host access.
    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    barrier.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
    native.CmdPipelineBarrier(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&barrier,0,nullptr,0,nullptr);
    auto metadata=planes(1920,1080);
    for (unsigned p=0;p<3;++p) {
        VkBufferImageCopy copy{}; copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
        copy.imageExtent={metadata[p].width,metadata[p].height,1};
        native.CmdCopyImageToBuffer(command,slot.images[p].image,VK_IMAGE_LAYOUT_GENERAL,slot.staging[p].buffer,1,&copy);
    }
    barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
    native.CmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&barrier,0,nullptr,0,nullptr);
    submit(slot.timeline,decode,consumer); wait(slot.timeline,consumer);
    slot.payload.complete(consumer);
    PyroWave::Pixels result; result.width=1920; result.height=1080;
    for (unsigned p=0;p<3;++p) {
        auto& r=slot.staging[p]; void* mapped=nullptr;
        check(native.MapMemory(owner.deviceHandle(),r.allocation,0,VK_WHOLE_SIZE,0,&mapped),"map test-only readback");
        VkResult invalidated=VK_SUCCESS;
        if (!r.coherent) {
            VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE}; range.memory=r.allocation; range.size=VK_WHOLE_SIZE;
            invalidated=native.InvalidateMappedMemoryRanges(owner.deviceHandle(),1,&range);
        }
        if (invalidated==VK_SUCCESS) {
            const auto* bytes=static_cast<const uint8_t*>(mapped); result.planes[p].assign(bytes,bytes+metadata[p].bytes);
        }
        native.UnmapMemory(owner.deviceHandle(),r.allocation);
        check(invalidated,"invalidate readback");
    }
    log("slot="+std::to_string(index)+" decode_complete_payload="+std::to_string(decode)+" consumer_complete_payload="+std::to_string(consumer));
    return result;
}
void Outputs::drain() {
    for (auto& s:slots) if (s.initialized) wait(s.timeline,s.payload.pending ? s.payload.decoded : s.payload.consumed);
    QueueLock::Guard guard(queueLock); check(vk.DeviceWaitIdle(owner.deviceHandle()),"offline final caller drain");
}
void Outputs::close() noexcept {
    // Orchestrator drains, destroys decoder/wrapper, then calls close(). Partial
    // setup has no codec work; synchronous transition submissions were already waited.
    auto device=owner.deviceHandle();
    for (auto& s:slots) {
        for (auto& r:s.images) { if(r.image) native.DestroyImage(device,r.image,nullptr); if(r.allocation) native.FreeMemory(device,r.allocation,nullptr); r={}; }
        for (auto& r:s.staging) { if(r.buffer) native.DestroyBuffer(device,r.buffer,nullptr); if(r.allocation) native.FreeMemory(device,r.allocation,nullptr); r={}; }
        if(s.timeline) vk.DestroySemaphore(device,s.timeline,nullptr); s.timeline=VK_NULL_HANDLE;
    }
    if(pool) vk.DestroyCommandPool(device,pool,nullptr); pool=VK_NULL_HANDLE;
}
}
