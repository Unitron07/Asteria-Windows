#pragma once
#include "shared_device.h"
#include "stage4_policy.h"
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
#ifdef STAGE4_RESOURCE_TEST
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
    std::array<Slot,4> slots{};
    std::array<VkSampler,2> samplers{};
    VkDescriptorSetLayout descriptors{};
    VkDescriptorPool descriptorPool{};
    std::array<VkDescriptorSet,8> sets{};
    VkPipelineLayout layout{};
    std::array<VkShaderModule,2> modules{};
    std::array<VkPipeline,2> pipelines{};
    VkRenderPass verifyPass{};
    VkFramebuffer verifyFramebuffer{};
    Resource target{}, staging{}, upload{};
    VkCommandPool pool{};
    VkCommandBuffer command{};
    VkFence complete{};
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
              Stage3::QueueLock& q,PyroWaveVulkan::Log logger,Stage3::Path p,std::string fail);
    ~Presenter();
    void initialize();
    pyrowave_gpu_buffers views(unsigned slot) const;
    pyrowave_gpu_sync_operation acquire(unsigned slot) const;
    pyrowave_gpu_sync_operation release(unsigned slot) const;
    void decoded(unsigned slot,PyroWave::YuvRange range,Filter filter);
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
