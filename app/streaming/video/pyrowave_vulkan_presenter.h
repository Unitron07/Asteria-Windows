#pragma once
#include "pyrowave_vulkan_shared.h"
#include "pyrowave_vulkan_video_policy.h"
#include <fstream>

namespace Stage4 {
#define STAGE4_DEVICE(X) X(CreateShaderModule) X(DestroyShaderModule) X(CreateSampler) X(DestroySampler) \
    X(CreateDescriptorSetLayout) X(DestroyDescriptorSetLayout) X(CreateDescriptorPool) X(DestroyDescriptorPool) \
    X(AllocateDescriptorSets) X(UpdateDescriptorSets) X(CreatePipelineLayout) X(DestroyPipelineLayout) \
    X(CreateGraphicsPipelines) X(DestroyPipeline) X(CmdSetViewport) X(CmdSetScissor) \
    X(CmdBindPipeline) X(CmdBindDescriptorSets) X(CmdPushConstants) X(CmdDraw) \
    X(CmdCopyBufferToImage) X(FlushMappedMemoryRanges)
struct Dispatch {
#define DECLARE(name) PFN_vk##name name = nullptr;
    STAGE4_DEVICE(DECLARE)
#undef DECLARE
    void load(PyroWaveVulkan::Dispatch& vk,VkDevice device);
};
class Presenter {
#if defined(STAGE4_RESOURCE_TEST) || defined(STAGE5_RESOURCE_TEST)
    friend struct PresenterTestAccess;
#endif
    PyroWaveVulkan::Probe& owner;
    PyroWaveVulkan::Dispatch& vk;
    Stage3::NativeDispatch& native;
    Dispatch gpu;
    Stage3::QueueLock& queue;
    PyroWaveVulkan::Log log;
    Stage3::Path path;
    std::string fault;
    VkPhysicalDeviceMemoryProperties memory{};
    struct Resource { VkImage image{}; VkImageView view{}; VkBuffer buffer{}; VkDeviceMemory allocation{}; bool coherent=false; };
    struct Slot { std::array<Resource,3> planes{}; VkSemaphore timeline{}; Stage3::Payloads payload; };
    // Fourth set exists ONLY for exact-plane shader verification, with TRANSFER_DST.
    #ifdef PYROWAVE_VULKAN_VERIFIER
    static constexpr unsigned PlaneSets=4;
#else
    static constexpr unsigned PlaneSets=3;
#endif
    std::array<Slot,PlaneSets> slots{};
    std::array<VkSampler,2> samplers{};
    VkDescriptorSetLayout descriptors{};
    VkDescriptorPool descriptorPool{};
    std::array<VkDescriptorSet,PlaneSets*2> sets{};
    VkPipelineLayout layout{};
    std::array<VkShaderModule,2> modules{};
    std::array<VkPipeline,2> pipelines{};
    VkRenderPass verifyPass{};
    VkFramebuffer verifyFramebuffer{};
    Resource target{}, staging{}, upload{};
    VkCommandPool pool{};
    VkCommandBuffer command{};
    VkFence complete{};
    int width=1920,height=1080;
    bool live=false, ready=true;
    unsigned activeSlot=0;
    Filter activeFilter=Filter::Linear;
    PyroWave::YuvRange activeRange=PyroWave::YuvRange::Full;
    std::array<VkSemaphore,2> waits{}, signals{};
    std::array<uint64_t,2> waitValues{}, signalValues{};
    std::array<VkPipelineStageFlags,2> waitStages{};
    VkTimelineSemaphoreSubmitInfo timeline{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
    void checkpoint(const char* name);
    void allocate(Resource& r,VkMemoryRequirements requirements,VkMemoryPropertyFlags required);
    void image(Resource& r,VkFormat format,VkExtent2D extent,VkImageUsageFlags usage);
    void buffer(Resource& r,VkDeviceSize bytes,VkBufferUsageFlags usage);
    void begin();
    void finish(VkSemaphore semaphore=VK_NULL_HANDLE,uint64_t signal=0);
    void destroy(Resource& r) noexcept;
    void draw(VkCommandBuffer cmd,VkRenderPass pass,VkFormat format,VkExtent2D extent,unsigned set,Filter filter,PyroWave::YuvRange range);
    void exactUpload(const PyroWave::Pixels& p);
public:
    Presenter(PyroWaveVulkan::Probe& o,PyroWaveVulkan::Dispatch& v,Stage3::NativeDispatch& n,
              Stage3::QueueLock& q,PyroWaveVulkan::Log logger,Stage3::Path p,std::string fail,int w=1920,int h=1080,bool liveMode=false);
    ~Presenter();
    void initialize();
    pyrowave_gpu_buffers views(unsigned slot) const;
    pyrowave_gpu_sync_operation acquire(unsigned slot) const;
    pyrowave_gpu_sync_operation release(unsigned slot) const;
    void decoded(unsigned slot,PyroWave::YuvRange range,Filter filter);
    // Caller serializes these methods with decode/publication. Retirement has no
    // command buffer and may run on the decoder thread, using the same queue lock.
    void select(unsigned slot,PyroWave::YuvRange range,Filter filter);
    bool retire(unsigned slot);
    VkSemaphore slotTimeline(unsigned slot) const { return slots.at(slot).timeline; }
    uint64_t consumerValue(unsigned slot) const { return slots.at(slot).payload.consumed; }
    void before(VkCommandBuffer cmd);
    void record(VkCommandBuffer cmd,VkRenderPass pass,VkExtent2D extent);
    void submission(VkSubmitInfo& submit);
    void submitted();
    void verify(const std::filesystem::path& evidence);
    void close() noexcept; // Caller drains and destroys codec wrapper first.
    unsigned maxRgbError=0;
    uint64_t verifiedPixels=0,chromaNegativeControls=0;
};
}
