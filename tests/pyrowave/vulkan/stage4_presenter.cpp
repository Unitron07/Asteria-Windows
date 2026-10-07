#include "stage4_presenter.h"
#include "shaders/video_spirv.h"
#include <cstring>
using PyroWaveVulkan::check;
namespace Stage4 {
void Dispatch::load(PyroWaveVulkan::Dispatch& vk,VkDevice device) {
#define LOAD(name) name=reinterpret_cast<PFN_vk##name>(vk.GetDeviceProcAddr(device,"vk" #name)); if(!name) throw std::runtime_error("missing Stage 4 export vk" #name);
    STAGE4_DEVICE(LOAD)
#undef LOAD
}
Presenter::Presenter(PyroWaveVulkan::Probe& o,PyroWaveVulkan::Dispatch& v,Stage3::NativeDispatch& n,
                     Stage3::QueueLock& q,PyroWaveVulkan::Log logger,Stage3::Path p,std::string fail)
    :owner(o),vk(v),native(n),queue(q),log(std::move(logger)),path(p),fault(std::move(fail)) {}
Presenter::~Presenter() { close(); }
void Presenter::checkpoint(const char* name) {
    if(fault==name) throw std::runtime_error(std::string("injected Stage 4 ")+name);
}
void Presenter::allocate(Resource& r,VkMemoryRequirements req,VkMemoryPropertyFlags required) {
    int selected=-1;
    for(unsigned i=0;i<memory.memoryTypeCount;++i) if((req.memoryTypeBits&(1u<<i)) &&
        (memory.memoryTypes[i].propertyFlags&required)==required) {
        if(selected<0) selected=int(i);
        const auto prefer=required ? VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        if(memory.memoryTypes[i].propertyFlags&prefer) { selected=int(i); break; }
    }
    if(selected<0) throw std::runtime_error("Stage 4 compatible memory unavailable");
    r.coherent=bool(memory.memoryTypes[selected].propertyFlags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VkMemoryAllocateInfo info{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; info.allocationSize=req.size; info.memoryTypeIndex=uint32_t(selected);
    check(native.AllocateMemory(owner.deviceHandle(),&info,nullptr,&r.allocation),"Stage 4 allocation");
}
void Presenter::image(Resource& r,VkFormat format,VkExtent2D extent,VkImageUsageFlags usage) {
    VkImageFormatProperties support{};
    check(native.GetPhysicalDeviceImageFormatProperties(owner.physicalHandle(),format,VK_IMAGE_TYPE_2D,
        VK_IMAGE_TILING_OPTIMAL,usage,0,&support),"Stage 4 exact image usage support");
    if(support.maxExtent.width<extent.width || support.maxExtent.height<extent.height || !(support.sampleCounts&VK_SAMPLE_COUNT_1_BIT))
        throw std::runtime_error("Stage 4 image extent unsupported");
    VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO}; info.imageType=VK_IMAGE_TYPE_2D;
    info.format=format; info.extent={extent.width,extent.height,1}; info.mipLevels=info.arrayLayers=1;
    info.samples=VK_SAMPLE_COUNT_1_BIT; info.tiling=VK_IMAGE_TILING_OPTIMAL; info.usage=usage; info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    check(native.CreateImage(owner.deviceHandle(),&info,nullptr,&r.image),"Stage 4 image");
    VkMemoryRequirements requirements{}; native.GetImageMemoryRequirements(owner.deviceHandle(),r.image,&requirements);
    allocate(r,requirements,0);
    check(native.BindImageMemory(owner.deviceHandle(),r.image,r.allocation,0),"Stage 4 bind image");
    VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO}; view.image=r.image;
    view.viewType=VK_IMAGE_VIEW_TYPE_2D; view.format=format; view.components={VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY};
    view.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    check(vk.CreateImageView(owner.deviceHandle(),&view,nullptr,&r.view),"Stage 4 sampled view");
    checkpoint("sampled-view");
}
void Presenter::buffer(Resource& r,VkDeviceSize bytes,VkBufferUsageFlags usage) {
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO}; info.size=bytes; info.usage=usage; info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    check(native.CreateBuffer(owner.deviceHandle(),&info,nullptr,&r.buffer),"verification-only buffer");
    VkMemoryRequirements req{}; native.GetBufferMemoryRequirements(owner.deviceHandle(),r.buffer,&req);
    allocate(r,req,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    check(native.BindBufferMemory(owner.deviceHandle(),r.buffer,r.allocation,0),"verification buffer memory");
}
void Presenter::begin() {
    check(vk.ResetCommandBuffer(command,0),"verification reset");
    VkCommandBufferBeginInfo info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; info.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    check(vk.BeginCommandBuffer(command,&info),"verification begin");
}
void Presenter::finish(VkSemaphore semaphore,uint64_t signal) {
    check(vk.EndCommandBuffer(command),"verification end");
    check(vk.ResetFences(owner.deviceHandle(),1,&complete),"verification reset fence");
    VkSubmitInfo info{VK_STRUCTURE_TYPE_SUBMIT_INFO}; info.commandBufferCount=1; info.pCommandBuffers=&command;
    VkTimelineSemaphoreSubmitInfo values{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
    if(semaphore) { values.signalSemaphoreValueCount=1; values.pSignalSemaphoreValues=&signal; info.pNext=&values;
        info.signalSemaphoreCount=1; info.pSignalSemaphores=&semaphore; }
    { Stage3::QueueLock::Guard lock(queue); check(vk.QueueSubmit(owner.queueHandle(),1,&info,complete),"verification/startup submit"); }
    check(vk.WaitForFences(owner.deviceHandle(),1,&complete,VK_TRUE,30000000000ull),"verification/startup completion");
}
void Presenter::initialize() {
    const auto device=owner.deviceHandle(); gpu.load(vk,device);
    native.GetPhysicalDeviceMemoryProperties(owner.physicalHandle(),&memory);
    VkFormatProperties properties{}; native.GetPhysicalDeviceFormatProperties(owner.physicalHandle(),VK_FORMAT_R8_UNORM,&properties);
    if(!(properties.optimalTilingFeatures&VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) throw std::runtime_error("R8 linear sampling unsupported; STOP");
    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; poolInfo.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; poolInfo.queueFamilyIndex=owner.queueFamilyIndex();
    check(vk.CreateCommandPool(device,&poolInfo,nullptr,&pool),"Stage 4 verifier pool");
    VkCommandBufferAllocateInfo alloc{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO}; alloc.commandPool=pool; alloc.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; alloc.commandBufferCount=1;
    check(vk.AllocateCommandBuffers(device,&alloc,&command),"Stage 4 verifier command");
    VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO}; check(vk.CreateFence(device,&fence,nullptr,&complete),"Stage 4 verifier fence");
    const auto planes=Stage3::planes(1920,1080);
    for(unsigned i=0;i<slots.size();++i) {
        auto& slot=slots[i];
        VkSemaphoreTypeCreateInfo type{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO}; type.semaphoreType=VK_SEMAPHORE_TYPE_TIMELINE;
        VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO}; semaphore.pNext=&type;
        check(vk.CreateSemaphore(device,&semaphore,nullptr,&slot.timeline),"Stage 4 nonexportable timeline");
        const auto usage=i<3 ? outputUsage(path) : VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
        for(unsigned p=0;p<3;++p) image(slot.planes[p],VK_FORMAT_R8_UNORM,{planes[p].width,planes[p].height},usage);
        log("plane_set="+std::to_string(i)+" usage="+std::to_string(usage)+(i<3 ? " role=PRESENTATION no_staging=YES" : " role=TEST_VERIFICATION"));
        begin();
        std::array<VkImageMemoryBarrier,3> barriers{};
        for(unsigned p=0;p<3;++p) {
            auto& b=barriers[p]; b.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER; b.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED; b.newLayout=VK_IMAGE_LAYOUT_GENERAL;
            b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED; b.image=slot.planes[p].image;
            b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}; b.dstAccessMask=VK_ACCESS_MEMORY_WRITE_BIT;
        }
        native.CmdPipelineBarrier(command,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,nullptr,0,nullptr,3,barriers.data());
        finish(slot.timeline,1);
    }
    for(unsigned i=0;i<2;++i) {
        VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO}; sampler.magFilter=sampler.minFilter=i ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
        sampler.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler.addressModeU=sampler.addressModeV=sampler.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler.maxLod=0; sampler.anisotropyEnable=VK_FALSE; sampler.maxAnisotropy=1;
        check(gpu.CreateSampler(device,&sampler,nullptr,&samplers[i]),"Stage 4 explicit sampler"); checkpoint("sampler");
    }
    std::array<VkDescriptorSetLayoutBinding,3> bindings{};
    for(unsigned p=0;p<3;++p) bindings[p]={p,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr};
    VkDescriptorSetLayoutCreateInfo dsl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO}; dsl.bindingCount=3; dsl.pBindings=bindings.data();
    check(gpu.CreateDescriptorSetLayout(device,&dsl,nullptr,&descriptors),"Stage 4 descriptor layout"); checkpoint("descriptor-layout");
    VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,24};
    VkDescriptorPoolCreateInfo dpi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO}; dpi.maxSets=8; dpi.poolSizeCount=1; dpi.pPoolSizes=&size;
    check(gpu.CreateDescriptorPool(device,&dpi,nullptr,&descriptorPool),"Stage 4 descriptor pool"); checkpoint("descriptor-pool");
    std::array<VkDescriptorSetLayout,8> layouts; layouts.fill(descriptors);
    VkDescriptorSetAllocateInfo dai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO}; dai.descriptorPool=descriptorPool; dai.descriptorSetCount=8; dai.pSetLayouts=layouts.data();
    check(gpu.AllocateDescriptorSets(device,&dai,sets.data()),"Stage 4 static descriptor sets"); checkpoint("descriptor-sets");
    for(unsigned i=0;i<8;++i) for(unsigned p=0;p<3;++p) {
        VkDescriptorImageInfo img{samplers[i%2],slots[i/2].planes[p].view,VK_IMAGE_LAYOUT_GENERAL};
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}; write.dstSet=sets[i]; write.dstBinding=p; write.descriptorCount=1; write.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; write.pImageInfo=&img;
        gpu.UpdateDescriptorSets(device,1,&write,0,nullptr);
    }
    VkPushConstantRange range{VK_SHADER_STAGE_FRAGMENT_BIT,0,6*sizeof(int32_t)};
    VkPipelineLayoutCreateInfo pli{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO}; pli.setLayoutCount=1; pli.pSetLayouts=&descriptors; pli.pushConstantRangeCount=1; pli.pPushConstantRanges=&range;
    check(gpu.CreatePipelineLayout(device,&pli,nullptr,&layout),"Stage 4 pipeline layout"); checkpoint("pipeline-layout");
    for(unsigned i=0;i<2;++i) {
        VkShaderModuleCreateInfo module{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        module.codeSize=i ? sizeof(VideoFragment) : sizeof(VideoVertex); module.pCode=i ? VideoFragment : VideoVertex;
        check(gpu.CreateShaderModule(device,&module,nullptr,&modules[i]),"Stage 4 embedded shader"); checkpoint("shader-module");
    }
    // TEST VERIFICATION ONLY. Swapchain TRANSFER_SRC is neither queried nor used.
    image(target,VK_FORMAT_R8G8B8A8_UNORM,{1920,1080},VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    buffer(staging,1920ull*1080*4,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    buffer(upload,1920ull*1080*3/2,VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    VkAttachmentDescription attachment{}; attachment.format=VK_FORMAT_R8G8B8A8_UNORM; attachment.samples=VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR; attachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE; attachment.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED; attachment.finalLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    VkAttachmentReference ref{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{}; subpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS; subpass.colorAttachmentCount=1; subpass.pColorAttachments=&ref;
    VkSubpassDependency dependency{}; dependency.srcSubpass=VK_SUBPASS_EXTERNAL; dependency.dstSubpass=0;
    dependency.srcStageMask=dependency.dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; dependency.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO}; rp.attachmentCount=1; rp.pAttachments=&attachment; rp.subpassCount=1; rp.pSubpasses=&subpass;
    rp.dependencyCount=1; rp.pDependencies=&dependency;
    check(vk.CreateRenderPass(device,&rp,nullptr,&verifyPass),"verification render pass");
    VkFramebufferCreateInfo fb{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO}; fb.renderPass=verifyPass; fb.attachmentCount=1; fb.pAttachments=&target.view; fb.width=1920; fb.height=1080; fb.layers=1;
    check(vk.CreateFramebuffer(device,&fb,nullptr,&verifyFramebuffer),"verification framebuffer"); checkpoint("offscreen-target");
}
pyrowave_gpu_buffers Presenter::views(unsigned index) const {
    if(index>=3) throw std::out_of_range("presentation slot");
    pyrowave_gpu_buffers result{}; const auto planes=Stage3::planes(1920,1080);
    for(unsigned p=0;p<3;++p) { auto& v=result.planes[p]; v.image=slots[index].planes[p].image;
        v.width=planes[p].width; v.height=planes[p].height; v.image_format=v.view_format=VK_FORMAT_R8_UNORM;
        v.aspect=VK_IMAGE_ASPECT_COLOR_BIT; v.swizzle=VK_COMPONENT_SWIZZLE_IDENTITY; v.layout=VK_IMAGE_LAYOUT_GENERAL; }
    return result;
}
pyrowave_gpu_sync_operation Presenter::acquire(unsigned index) const { pyrowave_gpu_sync_operation op{}; op.sync={slots.at(index).timeline,slots.at(index).payload.next()[0]}; return op; }
pyrowave_gpu_sync_operation Presenter::release(unsigned index) const { pyrowave_gpu_sync_operation op{}; op.sync={slots.at(index).timeline,slots.at(index).payload.next()[1]}; return op; }
void Presenter::decoded(unsigned index,PyroWave::YuvRange range,Filter filter) {
    if(index>=3) throw std::out_of_range("presentation slot");
    auto& slot=slots[index]; slot.payload.submitted(slot.payload.next()[1]); activeSlot=index; activeRange=range; activeFilter=filter;
}
void Presenter::before(VkCommandBuffer cmd) {
    if(!slots.at(activeSlot).payload.pending) throw std::runtime_error("sampling without decode");
    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER}; barrier.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; barrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
    native.CmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,1,&barrier,0,nullptr,0,nullptr);
}
void Presenter::submission(VkSubmitInfo& info) {
    auto& s=slots.at(activeSlot); if(!s.payload.pending) throw std::runtime_error("missing decode payload");
    waits={info.pWaitSemaphores[0],s.timeline}; signals={info.pSignalSemaphores[0],s.timeline};
    waitValues={0,s.payload.decoded}; signalValues={0,s.payload.decoded+1};
    waitStages={VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};
    timeline.waitSemaphoreValueCount=timeline.signalSemaphoreValueCount=2; timeline.pWaitSemaphoreValues=waitValues.data(); timeline.pSignalSemaphoreValues=signalValues.data();
    info.pNext=&timeline; info.waitSemaphoreCount=info.signalSemaphoreCount=2; info.pWaitSemaphores=waits.data(); info.pSignalSemaphores=signals.data(); info.pWaitDstStageMask=waitStages.data();
}
void Presenter::submitted() {
    auto& slot=slots.at(activeSlot); const auto decoded=slot.payload.decoded; slot.payload.complete(decoded+1);
    log("PRESENTATION slot="+std::to_string(activeSlot)+" decode_wait="+std::to_string(decoded)+" consumer_gpu_signal="+std::to_string(decoded+1)+" wait_stage=FRAGMENT_SHADER cpu_yuv_readback=NO");
}
void Presenter::record(VkCommandBuffer cmd,VkRenderPass pass,VkExtent2D extent) { draw(cmd,pass,owner.swapchainFormat,extent,activeSlot,activeFilter,activeRange); }
void Presenter::draw(VkCommandBuffer cmd,VkRenderPass pass,VkFormat format,VkExtent2D extent,unsigned set,Filter filter,PyroWave::YuvRange range) {
    const unsigned index=format==VK_FORMAT_R8G8B8A8_UNORM ? 0 : 1;
    if(!sdrTarget(format,VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)) throw std::runtime_error("non-UNORM video target");
    if(!pipelines[index]) {
        std::array<VkPipelineShaderStageCreateInfo,2> stages{};
        for(unsigned i=0;i<2;++i) { stages[i].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stages[i].stage=i ? VK_SHADER_STAGE_FRAGMENT_BIT : VK_SHADER_STAGE_VERTEX_BIT; stages[i].module=modules[i]; stages[i].pName="main"; }
        VkPipelineVertexInputStateCreateInfo vertex{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO}; assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO}; viewport.viewportCount=viewport.scissorCount=1;
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO}; raster.polygonMode=VK_POLYGON_MODE_FILL; raster.cullMode=VK_CULL_MODE_NONE; raster.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE; raster.lineWidth=1;
        VkPipelineMultisampleStateCreateInfo samples{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO}; samples.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState attachment{}; attachment.colorWriteMask=15;
        VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO}; blend.attachmentCount=1; blend.pAttachments=&attachment;
        std::array<VkDynamicState,2> dynamic{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO}; ds.dynamicStateCount=2; ds.pDynamicStates=dynamic.data();
        VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO}; info.stageCount=2; info.pStages=stages.data();
        info.pVertexInputState=&vertex; info.pInputAssemblyState=&assembly; info.pViewportState=&viewport; info.pRasterizationState=&raster; info.pMultisampleState=&samples; info.pColorBlendState=&blend; info.pDynamicState=&ds; info.layout=layout; info.renderPass=pass;
        check(gpu.CreateGraphicsPipelines(owner.deviceHandle(),VK_NULL_HANDLE,1,&info,nullptr,&pipelines[index]),"Stage 4 pipeline"); checkpoint("pipeline");
    }
    const auto fit=Presentation::fit(int(extent.width),int(extent.height)); if(!fit.w || !fit.h) return;
    VkViewport viewport{float(fit.x),float(fit.y),float(fit.w),float(fit.h),0,1};
    VkRect2D scissor{{fit.x,fit.y},{uint32_t(fit.w),uint32_t(fit.h)}};
    gpu.CmdSetViewport(cmd,0,1,&viewport); gpu.CmdSetScissor(cmd,0,1,&scissor);
    gpu.CmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelines[index]);
    const auto descriptorSet=sets.at(set*2+(filter==Filter::Linear ? 1 : 0));
    gpu.CmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,layout,0,1,&descriptorSet,0,nullptr);
    std::array<int32_t,6> params{fit.x,fit.y,fit.w,fit.h,range==PyroWave::YuvRange::Limited ? 1 : 0,filter==Filter::Nearest ? 1 : 0};
    gpu.CmdPushConstants(cmd,layout,VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(params),params.data());
    gpu.CmdDraw(cmd,3,1,0,0);
}
void Presenter::exactUpload(const PyroWave::Pixels& p) {
    void* mapped=nullptr; check(native.MapMemory(owner.deviceHandle(),upload.allocation,0,VK_WHOLE_SIZE,0,&mapped),"test exact-plane upload map");
    size_t offset=0; for(const auto& plane:p.planes) { std::memcpy(static_cast<uint8_t*>(mapped)+offset,plane.data(),plane.size()); offset+=plane.size(); }
    VkResult flushed=VK_SUCCESS;
    if(!upload.coherent) { VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE}; range.memory=upload.allocation; range.size=VK_WHOLE_SIZE; flushed=gpu.FlushMappedMemoryRanges(owner.deviceHandle(),1,&range); }
    native.UnmapMemory(owner.deviceHandle(),upload.allocation); check(flushed,"test upload flush");
    begin();
    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER}; barrier.srcAccessMask=VK_ACCESS_SHADER_READ_BIT; barrier.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
    native.CmdPipelineBarrier(command,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&barrier,0,nullptr,0,nullptr);
    const auto planes=Stage3::planes(1920,1080); offset=0;
    for(unsigned i=0;i<3;++i) { VkBufferImageCopy copy{}; copy.bufferOffset=offset; copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}; copy.imageExtent={planes[i].width,planes[i].height,1};
        gpu.CmdCopyBufferToImage(command,upload.buffer,slots[3].planes[i].image,VK_IMAGE_LAYOUT_GENERAL,1,&copy); offset+=planes[i].bytes; }
    barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
    native.CmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,1,&barrier,0,nullptr,0,nullptr);
    finish();
}
void Presenter::verify(const std::filesystem::path& evidence) {
    std::ofstream results(evidence/"shader-verification.json"),points(evidence/"color-reference.json");
    if(!results || !points) throw std::runtime_error("verification evidence unavailable");
    results << "{\"path\":\"TEST_VERIFICATION_ONLY\",\"rgbTolerance\":1,\"cases\":[";
    points << "{\"reference\":\"independent Kr/Kb double arithmetic\",\"observations\":[";
    bool first=true,firstPoint=true;
    for(auto range:{PyroWave::YuvRange::Full,PyroWave::YuvRange::Limited})
    for(auto pattern:{Presentation::Pattern::Range,Presentation::Pattern::Bars,Presentation::Pattern::Chroma,Presentation::Pattern::Geometry,Presentation::Pattern::Gradient}) {
        const auto p=fixture(pattern,range); exactUpload(p);
        for(auto filter:{Filter::Nearest,Filter::Linear}) for(auto extent:{VkExtent2D{1920,1080},VkExtent2D{1001,751},VkExtent2D{1801,700},VkExtent2D{127,93}}) {
            begin(); VkClearValue black{}; black.color.float32[3]=1;
            VkRenderPassBeginInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO}; pass.renderPass=verifyPass; pass.framebuffer=verifyFramebuffer; pass.renderArea.extent=extent; pass.clearValueCount=1; pass.pClearValues=&black;
            vk.CmdBeginRenderPass(command,&pass,VK_SUBPASS_CONTENTS_INLINE); draw(command,verifyPass,VK_FORMAT_R8G8B8A8_UNORM,extent,3,filter,range); vk.CmdEndRenderPass(command);
            VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER}; barrier.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; barrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
            native.CmdPipelineBarrier(command,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&barrier,0,nullptr,0,nullptr);
            VkBufferImageCopy copy{}; copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}; copy.imageExtent={extent.width,extent.height,1};
            native.CmdCopyImageToBuffer(command,target.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,staging.buffer,1,&copy);
            barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
            native.CmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&barrier,0,nullptr,0,nullptr); finish();
            void* mapped=nullptr; check(native.MapMemory(owner.deviceHandle(),staging.allocation,0,VK_WHOLE_SIZE,0,&mapped),"test RGB readback map");
            VkResult invalidated=VK_SUCCESS;
            if(!staging.coherent) { VkMappedMemoryRange mr{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE}; mr.memory=staging.allocation; mr.size=VK_WHOLE_SIZE; invalidated=native.InvalidateMappedMemoryRanges(owner.deviceHandle(),1,&mr); }
            std::vector<uint8_t> rgb;
            if(invalidated==VK_SUCCESS) rgb.assign(static_cast<uint8_t*>(mapped),static_cast<uint8_t*>(mapped)+size_t(extent.width)*extent.height*4);
            native.UnmapMemory(owner.deviceHandle(),staging.allocation); check(invalidated,"test RGB invalidate");
            const auto fit=Presentation::fit(int(extent.width),int(extent.height)); unsigned maximum=0; uint64_t mismatches=0,leftDifferences=0;
            for(unsigned y=0;y<extent.height;++y) for(unsigned x=0;x<extent.width;++x) {
                Rgb expected{}; bool inside=int(x)>=fit.x && int(y)>=fit.y && int(x)<fit.x+fit.w && int(y)<fit.y+fit.h;
                const double u=inside ? (double(x)-fit.x+0.5)/fit.w : 0, v=inside ? (double(y)-fit.y+0.5)/fit.h : 0;
                if(inside && filter==Filter::Nearest) {
                    const auto lx=(uint64_t(2*(int(x)-fit.x)+1)*1920)/(2*fit.w),ly=(uint64_t(2*(int(y)-fit.y)+1)*1080)/(2*fit.h);
                    const auto cx=(uint64_t(2*(int(x)-fit.x)+1)*960)/(2*fit.w),cy=(uint64_t(2*(int(y)-fit.y)+1)*540)/(2*fit.h);
                    expected=reference(p.planes[0].at(ly*1920+lx),p.planes[1].at(cy*960+cx),p.planes[2].at(cy*960+cx),p.range);
                } else if(inside) expected=referencePixel(p,u,v,filter);
                const size_t offset=(size_t(y)*extent.width+x)*4;
                for(unsigned c=0;c<3;++c) { const unsigned error=unsigned(std::abs(int(rgb[offset+c])-expected[c])); maximum=std::max(maximum,error); if(error>RgbTolerance) ++mismatches; }
                if(rgb[offset+3]!=255) ++mismatches;
                if(inside && pattern==Presentation::Pattern::Chroma) {
                    const auto left=referencePixel(p,u,v,filter,true);
                    for(unsigned c=0;c<3;++c) if(std::abs(int(left[c])-expected[c])>2*int(RgbTolerance)) { ++leftDifferences; break; }
                }
                if(inside && y==unsigned(fit.y+fit.h/2) && x%std::max(1u,extent.width/16)==0) {
                    if(!firstPoint) points<<','; firstPoint=false;
                    points<<"{\"pattern\":\""<<Presentation::name(pattern)<<"\",\"range\":\""<<(range==PyroWave::YuvRange::Full ? "FULL" : "LIMITED")<<"\",\"x\":"<<x<<",\"y\":"<<y<<",\"expected\":["<<int(expected[0])<<','<<int(expected[1])<<','<<int(expected[2])<<"],\"observed\":["<<int(rgb[offset])<<','<<int(rgb[offset+1])<<','<<int(rgb[offset+2])<<"]}";
                }
            }
            maxRgbError=std::max(maxRgbError,maximum); verifiedPixels+=uint64_t(extent.width)*extent.height; chromaNegativeControls+=leftDifferences;
            if(!first) results<<','; first=false;
            results<<"{\"pattern\":\""<<Presentation::name(pattern)<<"\",\"range\":\""<<(range==PyroWave::YuvRange::Full ? "FULL" : "LIMITED")<<"\",\"filter\":\""<<(filter==Filter::Linear ? "LINEAR" : "NEAREST")<<"\",\"width\":"<<extent.width<<",\"height\":"<<extent.height<<",\"maxError\":"<<maximum<<",\"failedComponents\":"<<mismatches<<",\"leftNegativeControlPixels\":"<<leftDifferences<<"}";
            log("TEST_VERIFICATION pattern="+std::string(Presentation::name(pattern))+" max_rgb_error="+std::to_string(maximum)+" failed_components="+std::to_string(mismatches));
            if(mismatches) { results<<"],\"pass\":false}"; points<<"]}"; throw std::runtime_error("shader RGB tolerance failure; STOP"); }
        }
    }
    results<<"],\"maxError\":"<<maxRgbError<<",\"verifiedPixels\":"<<verifiedPixels<<",\"chromaNegativeControlPixels\":"<<chromaNegativeControls<<",\"pass\":"<<(chromaNegativeControls ? "true" : "false")<<"}"; points<<"]}";
    if(!chromaNegativeControls) throw std::runtime_error("ambiguous CENTER/LEFT test; STOP");
}
void Presenter::destroy(Resource& r) noexcept {
    const auto device=owner.deviceHandle(); if(r.view) vk.DestroyImageView(device,r.view,nullptr); if(r.image) native.DestroyImage(device,r.image,nullptr);
    if(r.buffer) native.DestroyBuffer(device,r.buffer,nullptr); if(r.allocation) native.FreeMemory(device,r.allocation,nullptr); r={};
}
void Presenter::close() noexcept {
    const auto device=owner.deviceHandle(); if(!device) return;
    for(auto& pipeline:pipelines) { if(pipeline) gpu.DestroyPipeline(device,pipeline,nullptr); pipeline={}; }
    if(verifyFramebuffer) vk.DestroyFramebuffer(device,verifyFramebuffer,nullptr); verifyFramebuffer={};
    if(verifyPass) vk.DestroyRenderPass(device,verifyPass,nullptr); verifyPass={};
    if(descriptorPool) gpu.DestroyDescriptorPool(device,descriptorPool,nullptr); descriptorPool={};
    if(layout) gpu.DestroyPipelineLayout(device,layout,nullptr); layout={};
    if(descriptors) gpu.DestroyDescriptorSetLayout(device,descriptors,nullptr); descriptors={};
    for(auto& module:modules) { if(module) gpu.DestroyShaderModule(device,module,nullptr); module={}; }
    for(auto& sampler:samplers) { if(sampler) gpu.DestroySampler(device,sampler,nullptr); sampler={}; }
    for(auto& slot:slots) { for(auto& resource:slot.planes) destroy(resource); if(slot.timeline) vk.DestroySemaphore(device,slot.timeline,nullptr); slot.timeline={}; }
    destroy(target); destroy(staging); destroy(upload);
    if(complete) vk.DestroyFence(device,complete,nullptr); complete={};
    if(pool) vk.DestroyCommandPool(device,pool,nullptr); pool={};
}
}
