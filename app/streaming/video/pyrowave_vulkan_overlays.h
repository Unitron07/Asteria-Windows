#pragma once
#include "pyrowave_vulkan_presenter.h"
#include "pyrowave_vulkan_live_policy.h"
#include <SDL.h>
#include <optional>

namespace PyroWaveVulkan {
// Three reusable generations per surface. An in-flight generation is never
// rewritten, rebound or destroyed. Replacement waits only by deferring upload;
// the newest bounded CPU update replaces older not-yet-uploaded updates.
class Overlays {
#ifdef STAGE5_RESOURCE_TEST
    friend struct OverlayTestAccess;
#endif
    Probe& owner;
    PyroWaveVulkan::Dispatch& vk;
    Stage3::NativeDispatch& native;
    Stage4::Dispatch gpu;
    VkPhysicalDeviceMemoryProperties memory{};
    VkSampler sampler{};
    VkDescriptorSetLayout descriptors{};
    VkDescriptorPool descriptorPool{};
    VkPipelineLayout layout{};
    VkShaderModule vertex{},fragment{};
    std::array<VkPipeline,2> pipelines{};
    struct Generation {
        VkImage image{}; VkImageView view{}; VkDeviceMemory imageMemory{};
        VkBuffer staging{}; VkDeviceMemory stagingMemory{};
        VkDescriptorSet set{};
        VkSemaphore lastTimeline{}; uint64_t lastValue=0;
        int width=0,height=0; bool coherent=false,initialized=false,dirty=false;
    };
    struct Surface {
        std::array<Generation,3> generations{};
        int current=-1,width=0,height=0;
        bool enabled=false,updated=false;
        std::vector<uint8_t> rgba;
    };
    std::array<Surface,2> surfaces{};
    PFN_vkGetSemaphoreCounterValue counter=nullptr;
    bool available(const Generation&) const;
    void destroy(Generation&) noexcept;
    void allocate(Generation&,int,int);
    void prepare(Surface&);
public:
    Overlays(Probe& o,PyroWaveVulkan::Dispatch& v,Stage3::NativeDispatch& n):owner(o),vk(v),native(n) {}
    ~Overlays() { close(); }
    void initialize();
    void update(unsigned index,SDL_Surface* surface,bool enabled);
    void before(VkCommandBuffer command);
    void record(VkCommandBuffer command,VkRenderPass pass,VkExtent2D extent);
    void submitted(VkSemaphore timeline,uint64_t value);
    bool pending() const { for(const auto& s:surfaces) if(s.enabled && s.updated) return true; return false; }
    void close() noexcept; // Caller drains the queue first.
    uint64_t uploads=0;
};
}
