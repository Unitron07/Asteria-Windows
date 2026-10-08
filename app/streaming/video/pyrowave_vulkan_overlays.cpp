#include "pyrowave_vulkan_overlays.h"
#include "shaders/pyrowave/video_spirv.h"
#include "shaders/pyrowave/overlay_spirv.h"
#include <cstring>

namespace PyroWaveVulkan {
void Overlays::initialize() {
    const auto device=owner.deviceHandle(); gpu.load(vk,device);
    native.GetPhysicalDeviceMemoryProperties(owner.physicalHandle(),&memory);
    counter=reinterpret_cast<PFN_vkGetSemaphoreCounterValue>(vk.GetDeviceProcAddr(device,"vkGetSemaphoreCounterValue"));
    if(!counter) throw std::runtime_error("overlay timeline query unavailable");
    VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    si.magFilter=si.minFilter=VK_FILTER_NEAREST; si.addressModeU=si.addressModeV=si.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    check(gpu.CreateSampler(device,&si,nullptr,&sampler),"overlay sampler");
    VkDescriptorSetLayoutBinding binding{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr};
    VkDescriptorSetLayoutCreateInfo di{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO}; di.bindingCount=1; di.pBindings=&binding;
    check(gpu.CreateDescriptorSetLayout(device,&di,nullptr,&descriptors),"overlay descriptor layout");
    VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,6};
    VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO}; pi.maxSets=6; pi.poolSizeCount=1; pi.pPoolSizes=&size;
    check(gpu.CreateDescriptorPool(device,&pi,nullptr,&descriptorPool),"overlay descriptor pool");
    VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO}; li.setLayoutCount=1; li.pSetLayouts=&descriptors;
    check(gpu.CreatePipelineLayout(device,&li,nullptr,&layout),"overlay pipeline layout");
    VkShaderModuleCreateInfo mi{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO}; mi.codeSize=sizeof(Stage4::VideoVertex); mi.pCode=Stage4::VideoVertex;
    check(gpu.CreateShaderModule(device,&mi,nullptr,&vertex),"qualified triangle vertex module");
    mi.codeSize=sizeof(OverlayFragment); mi.pCode=OverlayFragment;
    check(gpu.CreateShaderModule(device,&mi,nullptr,&fragment),"separate overlay fragment module");
    for(auto& surface:surfaces) for(auto& g:surface.generations) {
        VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO}; ai.descriptorPool=descriptorPool; ai.descriptorSetCount=1; ai.pSetLayouts=&descriptors;
        check(gpu.AllocateDescriptorSets(device,&ai,&g.set),"persistent overlay descriptor");
    }
}
bool Overlays::available(const Generation& g) const {
    if(!g.lastTimeline) return true;
    uint64_t value=0; check(counter(owner.deviceHandle(),g.lastTimeline,&value),"overlay consumer completion");
    return value>=g.lastValue;
}
void Overlays::destroy(Generation& g) noexcept {
    const auto device=owner.deviceHandle();
    if(g.view) vk.DestroyImageView(device,g.view,nullptr);
    if(g.image) native.DestroyImage(device,g.image,nullptr);
    if(g.imageMemory) native.FreeMemory(device,g.imageMemory,nullptr);
    if(g.staging) native.DestroyBuffer(device,g.staging,nullptr);
    if(g.stagingMemory) native.FreeMemory(device,g.stagingMemory,nullptr);
    const auto set=g.set; g={}; g.set=set;
}
void Overlays::allocate(Generation& g,int width,int height) {
    if(g.width==width && g.height==height) return;
    destroy(g);
    const auto device=owner.deviceHandle();
    const auto allocate=[&](VkMemoryRequirements req,VkMemoryPropertyFlags required,VkDeviceMemory& result,bool* coherent=nullptr) {
        int selected=-1;
        for(unsigned i=0;i<memory.memoryTypeCount;++i) if((req.memoryTypeBits&(1u<<i)) && (memory.memoryTypes[i].propertyFlags&required)==required) {
            if(selected<0) selected=int(i);
            if(memory.memoryTypes[i].propertyFlags&(required ? VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) { selected=int(i); break; }
        }
        if(selected<0) throw std::runtime_error("overlay memory unavailable");
        if(coherent) *coherent=bool(memory.memoryTypes[selected].propertyFlags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        VkMemoryAllocateInfo mi{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; mi.allocationSize=req.size; mi.memoryTypeIndex=uint32_t(selected);
        check(native.AllocateMemory(device,&mi,nullptr,&result),"bounded overlay allocation");
    };
    VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO}; ii.imageType=VK_IMAGE_TYPE_2D; ii.format=VK_FORMAT_R8G8B8A8_UNORM;
    ii.extent={uint32_t(width),uint32_t(height),1}; ii.mipLevels=ii.arrayLayers=1; ii.samples=VK_SAMPLE_COUNT_1_BIT;
    ii.tiling=VK_IMAGE_TILING_OPTIMAL; ii.usage=VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
    check(native.CreateImage(device,&ii,nullptr,&g.image),"overlay RGBA image");
    VkMemoryRequirements req{}; native.GetImageMemoryRequirements(device,g.image,&req); allocate(req,0,g.imageMemory);
    check(native.BindImageMemory(device,g.image,g.imageMemory,0),"overlay image memory");
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO}; vi.image=g.image; vi.viewType=VK_IMAGE_VIEW_TYPE_2D;
    vi.format=ii.format; vi.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    check(vk.CreateImageView(device,&vi,nullptr,&g.view),"overlay image view");
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO}; bi.size=overlayBytes(width,height); bi.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    check(native.CreateBuffer(device,&bi,nullptr,&g.staging),"overlay staging buffer");
    native.GetBufferMemoryRequirements(device,g.staging,&req); allocate(req,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,g.stagingMemory,&g.coherent);
    check(native.BindBufferMemory(device,g.staging,g.stagingMemory,0),"overlay staging memory");
    VkDescriptorImageInfo image{sampler,g.view,VK_IMAGE_LAYOUT_GENERAL};
    VkWriteDescriptorSet wi{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}; wi.dstSet=g.set; wi.descriptorCount=1;
    wi.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; wi.pImageInfo=&image;
    gpu.UpdateDescriptorSets(device,1,&wi,0,nullptr);
    g.width=width; g.height=height;
}
bool Overlays::update(unsigned index,SDL_Surface* input,bool enabled) {
    auto& s=surfaces.at(index); const bool changed=s.enabled!=enabled; s.enabled=enabled;
    if(!enabled) { s.rgba.clear(); s.updated=false; s.current=-1; return changed; }
    if(!input) return changed;
    const size_t bytes=overlayBytes(input->w,input->h);
    SDL_Surface* rgba=SDL_ConvertSurfaceFormat(input,SDL_PIXELFORMAT_RGBA32,0);
    if(!rgba) throw std::runtime_error(std::string("overlay RGBA conversion: ")+SDL_GetError());
    std::unique_ptr<SDL_Surface,decltype(&SDL_FreeSurface)> free(rgba,SDL_FreeSurface);
    if(SDL_LockSurface(rgba)!=0) throw std::runtime_error(SDL_GetError());
    try {
        s.rgba.resize(bytes);
        for(int y=0;y<rgba->h;++y) std::memcpy(s.rgba.data()+size_t(y)*rgba->w*4,
            static_cast<const uint8_t*>(rgba->pixels)+size_t(y)*rgba->pitch,size_t(rgba->w)*4);
    } catch(...) { SDL_UnlockSurface(rgba); throw; }
    SDL_UnlockSurface(rgba); s.width=rgba->w; s.height=rgba->h; s.updated=true;
    return true;
}
void Overlays::prepare(Surface& s) {
    if(!s.enabled || !s.updated) return;
    for(unsigned i=0;i<s.generations.size();++i) {
        auto& g=s.generations[i]; if(!available(g)) continue;
        allocate(g,s.width,s.height);
        void* mapped=nullptr; check(native.MapMemory(owner.deviceHandle(),g.stagingMemory,0,VK_WHOLE_SIZE,0,&mapped),"overlay staging map");
        std::memcpy(mapped,s.rgba.data(),s.rgba.size());
        VkResult flushed=VK_SUCCESS;
        if(!g.coherent) { VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE}; range.memory=g.stagingMemory; range.size=VK_WHOLE_SIZE;
            flushed=gpu.FlushMappedMemoryRanges(owner.deviceHandle(),1,&range); }
        native.UnmapMemory(owner.deviceHandle(),g.stagingMemory); check(flushed,"overlay staging flush");
        g.dirty=true; s.current=int(i); s.updated=false; return;
    }
    // All generations in use: retain only the latest CPU update and current UI.
}
void Overlays::before(VkCommandBuffer command) {
    for(auto& s:surfaces) {
        prepare(s); if(!s.enabled || s.current<0) continue;
        auto& g=s.generations[s.current]; if(!g.dirty) continue;
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER}; barrier.image=g.image;
        barrier.oldLayout=g.initialized ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED; barrier.newLayout=VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED; barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        barrier.srcAccessMask=g.initialized ? VK_ACCESS_SHADER_READ_BIT : 0; barrier.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
        native.CmdPipelineBarrier(command,g.initialized ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
        VkBufferImageCopy copy{}; copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}; copy.imageExtent={uint32_t(g.width),uint32_t(g.height),1};
        gpu.CmdCopyBufferToImage(command,g.staging,g.image,VK_IMAGE_LAYOUT_GENERAL,1,&copy);
        barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
        native.CmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
    }
}
void Overlays::record(VkCommandBuffer command,VkRenderPass pass,VkExtent2D extent) {
    const unsigned index=owner.swapchainFormat==VK_FORMAT_R8G8B8A8_UNORM ? 0 : 1;
    if(!pipelines[index]) {
        std::array<VkPipelineShaderStageCreateInfo,2> stages{};
        for(unsigned i=0;i<2;++i) { stages[i].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stages[i].stage=i ? VK_SHADER_STAGE_FRAGMENT_BIT : VK_SHADER_STAGE_VERTEX_BIT; stages[i].module=i ? fragment : vertex; stages[i].pName="main"; }
        VkPipelineVertexInputStateCreateInfo vertexState{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO}; assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO}; viewport.viewportCount=viewport.scissorCount=1;
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO}; raster.polygonMode=VK_POLYGON_MODE_FILL; raster.cullMode=VK_CULL_MODE_NONE; raster.lineWidth=1;
        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO}; ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState alpha{}; alpha.colorWriteMask=15; alpha.blendEnable=VK_TRUE;
        alpha.srcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA; alpha.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; alpha.colorBlendOp=VK_BLEND_OP_ADD;
        alpha.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE; alpha.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; alpha.alphaBlendOp=VK_BLEND_OP_ADD;
        VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO}; blend.attachmentCount=1; blend.pAttachments=&alpha;
        std::array<VkDynamicState,2> states{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO}; ds.dynamicStateCount=2; ds.pDynamicStates=states.data();
        VkGraphicsPipelineCreateInfo pi{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO}; pi.stageCount=2; pi.pStages=stages.data();
        pi.pVertexInputState=&vertexState; pi.pInputAssemblyState=&assembly; pi.pViewportState=&viewport; pi.pRasterizationState=&raster;
        pi.pMultisampleState=&ms; pi.pColorBlendState=&blend; pi.pDynamicState=&ds; pi.layout=layout; pi.renderPass=pass;
        check(gpu.CreateGraphicsPipelines(owner.deviceHandle(),VK_NULL_HANDLE,1,&pi,nullptr,&pipelines[index]),"overlay alpha pipeline");
    }
    for(unsigned i=0;i<surfaces.size();++i) {
        auto& s=surfaces[i]; if(!s.enabled || s.current<0) continue;
        auto& g=s.generations[s.current]; const auto rect=overlayRect(g.width,g.height,int(extent.height),i==1);
        const int top=std::max(0,rect.y),bottom=std::min(int(extent.height),rect.y+g.height);
        if(bottom<=top) continue;
        VkViewport viewport{0,float(rect.y),float(g.width),float(g.height),0,1};
        VkRect2D scissor{{0,top},{std::min(extent.width,uint32_t(g.width)),uint32_t(bottom-top)}};
        gpu.CmdSetViewport(command,0,1,&viewport); gpu.CmdSetScissor(command,0,1,&scissor);
        gpu.CmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelines[index]);
        gpu.CmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_GRAPHICS,layout,0,1,&g.set,0,nullptr);
        gpu.CmdDraw(command,3,1,0,0);
    }
}
void Overlays::submitted(VkSemaphore timeline,uint64_t value) {
    for(auto& s:surfaces) if(s.enabled && s.current>=0) {
        auto& g=s.generations[s.current]; g.lastTimeline=timeline; g.lastValue=value;
        if(g.dirty) { ++uploads; g.dirty=false; g.initialized=true; }
    }
}
void Overlays::close() noexcept {
    const auto device=owner.deviceHandle(); if(!device) return;
    for(auto& s:surfaces) for(auto& g:s.generations) destroy(g);
    for(auto& p:pipelines) { if(p) gpu.DestroyPipeline(device,p,nullptr); p={}; }
    if(vertex) gpu.DestroyShaderModule(device,vertex,nullptr); vertex={};
    if(fragment) gpu.DestroyShaderModule(device,fragment,nullptr); fragment={};
    if(layout) gpu.DestroyPipelineLayout(device,layout,nullptr); layout={};
    if(descriptorPool) gpu.DestroyDescriptorPool(device,descriptorPool,nullptr); descriptorPool={};
    if(descriptors) gpu.DestroyDescriptorSetLayout(device,descriptors,nullptr); descriptors={};
    if(sampler) gpu.DestroySampler(device,sampler,nullptr); sampler={};
}
}
