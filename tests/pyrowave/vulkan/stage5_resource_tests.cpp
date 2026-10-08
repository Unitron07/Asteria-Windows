#define SDL_MAIN_HANDLED
#include "pyrowave_vulkan_overlays.h"
#include <iostream>
#include <thread>
#include <map>
#include <type_traits>
static void require(bool condition) { if(!condition) throw std::runtime_error("Stage 5 real resource assertion"); }
template<class F> void rejects(F operation) { bool rejected=false; try { operation(); } catch(const std::exception&) { rejected=true; } require(rejected); }
template<class T> T handle(uintptr_t n) { return reinterpret_cast<T>(n); }
namespace {
Stage3::QueueLock* queueLock=nullptr;
struct Submission { VkSemaphore semaphore; uint64_t wait,signal; };
std::vector<Submission> submissions;
bool submitFailure=false;
std::map<VkSemaphore,uint64_t> completed;
std::array<uint8_t,64> staging{};
unsigned barriers=0,copies=0;
std::map<uintptr_t,std::string> liveResources;
std::map<uintptr_t,uintptr_t> resourceParent;
bool cleanupOkay=true;
template<class T> void destroyTracked(T object,const char* kind) {
    const auto id=reinterpret_cast<uintptr_t>(object);
    if(!liveResources.count(id) || liveResources[id]!=kind) { cleanupOkay=false; return; }
    for(const auto& pair:resourceParent) if(pair.second==id && liveResources.count(pair.first)) cleanupOkay=false;
    liveResources.erase(id);
}
}
namespace PyroWaveVulkan {
struct ProbeTestAccess { static void device(Probe& p) { p.device=handle<VkDevice>(900); p.swapchainFormat=VK_FORMAT_R8G8B8A8_UNORM; } };
struct OverlayTestAccess {
    static unsigned seedCleanup(Overlays& o,unsigned prefix) {
        unsigned count=0;
        const auto add=[&](auto& field,const char* kind,uintptr_t dependency=0) {
            const auto id=uintptr_t(++count);
            if(count<=prefix) {
                field=handle<std::remove_reference_t<decltype(field)>>(id); liveResources[id]=kind;
                if(dependency) resourceParent[id]=dependency;
            }
            return id;
        };
        for(auto& s:o.surfaces) for(auto& g:s.generations) {
            const auto image=add(g.image,"image"),memory=add(g.imageMemory,"memory");
            if(memory<=prefix) resourceParent[image]=memory;
            add(g.view,"view",image);
            const auto buffer=add(g.staging,"buffer"),staging=add(g.stagingMemory,"memory");
            if(staging<=prefix) resourceParent[buffer]=staging;
        }
        add(o.sampler,"sampler"); add(o.descriptors,"descriptor-layout"); add(o.descriptorPool,"descriptor-pool");
        add(o.layout,"pipeline-layout"); add(o.vertex,"module"); add(o.fragment,"module");
        for(auto& pipeline:o.pipelines) add(pipeline,"pipeline");
        o.gpu.DestroyPipeline=[](VkDevice,VkPipeline h,const VkAllocationCallbacks*) { destroyTracked(h,"pipeline"); };
        o.gpu.DestroyDescriptorPool=[](VkDevice,VkDescriptorPool h,const VkAllocationCallbacks*) { destroyTracked(h,"descriptor-pool"); };
        o.gpu.DestroyDescriptorSetLayout=[](VkDevice,VkDescriptorSetLayout h,const VkAllocationCallbacks*) { destroyTracked(h,"descriptor-layout"); };
        o.gpu.DestroyPipelineLayout=[](VkDevice,VkPipelineLayout h,const VkAllocationCallbacks*) { destroyTracked(h,"pipeline-layout"); };
        o.gpu.DestroyShaderModule=[](VkDevice,VkShaderModule h,const VkAllocationCallbacks*) { destroyTracked(h,"module"); };
        o.gpu.DestroySampler=[](VkDevice,VkSampler h,const VkAllocationCallbacks*) { destroyTracked(h,"sampler"); };
        return count;
    }
    static void replacement(Overlays& o) {
        auto& g=o.surfaces[0].generations[0]; g.set=handle<VkDescriptorSet>(440);
        o.destroy(g); // Same destruction primitive used by extent replacement.
        require(!g.image && !g.staging && !g.stagingMemory && !g.width && g.set==handle<VkDescriptorSet>(440));
    }
    static void blendAndPlacement(Overlays& o) {
        unsigned draws=0; std::vector<float> positions;
        o.surfaces[0].enabled=true;
        o.surfaces[0].current=0;
        auto& status=o.surfaces[1]; status.enabled=true; status.current=0;
        status.generations[0].width=500; status.generations[0].height=10;
        o.gpu.CreateGraphicsPipelines=[](VkDevice,VkPipelineCache,uint32_t,const VkGraphicsPipelineCreateInfo* info,const VkAllocationCallbacks*,VkPipeline* out) {
            const auto& a=info->pColorBlendState->pAttachments[0];
            require(a.blendEnable && a.srcColorBlendFactor==VK_BLEND_FACTOR_SRC_ALPHA && a.dstColorBlendFactor==VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
            require(a.srcAlphaBlendFactor==VK_BLEND_FACTOR_ONE && a.dstAlphaBlendFactor==VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA && a.colorBlendOp==VK_BLEND_OP_ADD);
            *out=handle<VkPipeline>(999); return VK_SUCCESS;
        };
        o.gpu.DestroyPipeline=[](VkDevice,VkPipeline,const VkAllocationCallbacks*) {};
        // Capturing state lives across the synchronous real record() invocation.
        capturePositions=&positions; captureDraws=&draws;
        o.gpu.CmdSetViewport=[](VkCommandBuffer,uint32_t,uint32_t,const VkViewport* v) { capturePositions->push_back(v->y); };
        o.gpu.CmdSetScissor=[](VkCommandBuffer,uint32_t,uint32_t,const VkRect2D*) {};
        o.gpu.CmdBindPipeline=[](VkCommandBuffer,VkPipelineBindPoint,VkPipeline) {};
        o.gpu.CmdBindDescriptorSets=[](VkCommandBuffer,VkPipelineBindPoint,VkPipelineLayout,uint32_t,uint32_t,const VkDescriptorSet*,uint32_t,const uint32_t*) {};
        o.gpu.CmdDraw=[](VkCommandBuffer,uint32_t,uint32_t,uint32_t,uint32_t) { ++*captureDraws; };
        o.record({},handle<VkRenderPass>(500),{1280,720});
        require(draws==2 && positions==std::vector<float>({0,710}));
        capturePositions=nullptr; captureDraws=nullptr;
    }
    inline static std::vector<float>* capturePositions=nullptr;
    inline static unsigned* captureDraws=nullptr;
    static void inspect(Overlays& o) {
        auto& surface=o.surfaces[0];
        require(surface.width==2 && surface.height==1 && surface.updated);
        require(surface.rgba==std::vector<uint8_t>({1,2,3,127,4,5,6,255}));
        for(unsigned i=0;i<3;++i) {
            auto& g=surface.generations[i]; g.width=2; g.height=1;
            g.image=handle<VkImage>(100+i); g.stagingMemory=handle<VkDeviceMemory>(200+i);
            g.staging=handle<VkBuffer>(300+i); g.coherent=true;
        }
        o.counter=[](VkDevice,VkSemaphore semaphore,uint64_t* value) { *value=completed[semaphore]; return VK_SUCCESS; };
        o.gpu.CmdCopyBufferToImage=[](VkCommandBuffer,VkBuffer,VkImage,VkImageLayout,uint32_t,const VkBufferImageCopy*) { ++copies; };
    }
    static void state(Overlays& o,int current,bool pending) { require(o.surfaces[0].current==current && o.pending()==pending); }
};
}
namespace Stage4 {
struct PresenterTestAccess {
    static void seed(Presenter& p) { for(unsigned i=0;i<3;++i) p.slots[i].timeline=handle<VkSemaphore>(i+1); }
    static void overflow(Presenter& p) { p.slots[0].payload.consumed=UINT64_MAX; }
};
}
int main() {
    try {
        const auto historical=Stage4::fitVideo(1280,800);
        const auto negotiated=Stage4::fitVideo(1280,800,1440,1080);
        require(historical.x==0 && historical.y==40 && historical.w==1280 && historical.h==720);
        require(negotiated.x==107 && negotiated.y==0 && negotiated.w==1066 && negotiated.h==800);
        PyroWaveVulkan::Dispatch vk; Stage3::NativeDispatch native; Stage3::QueueLock queue; queueLock=&queue;
        vk.DestroyDevice=[](VkDevice,const VkAllocationCallbacks*) {};
        vk.DestroySemaphore=[](VkDevice,VkSemaphore,const VkAllocationCallbacks*) {};
        vk.QueueSubmit=[](VkQueue,uint32_t count,const VkSubmitInfo* info,VkFence) {
            require(count==1); rejects([] { queueLock->lock(true); }); // REAL retirement uses queue lock.
            if(submitFailure) return VK_ERROR_DEVICE_LOST;
            auto* values=static_cast<const VkTimelineSemaphoreSubmitInfo*>(info->pNext);
            require(values && values->sType==VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO);
            require(values->waitSemaphoreValueCount==info->waitSemaphoreCount && values->signalSemaphoreValueCount==info->signalSemaphoreCount);
            const auto i=info->waitSemaphoreCount-1;
            require(info->pWaitDstStageMask[i]==VK_PIPELINE_STAGE_ALL_COMMANDS_BIT || info->pWaitDstStageMask[i]==VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            submissions.push_back({info->pWaitSemaphores[i],values->pWaitSemaphoreValues[i],values->pSignalSemaphoreValues[i]});
            return VK_SUCCESS;
        };
        native.CmdPipelineBarrier=[](VkCommandBuffer,VkPipelineStageFlags,VkPipelineStageFlags,uint32_t,uint32_t,const VkMemoryBarrier*,uint32_t,const VkBufferMemoryBarrier*,uint32_t,const VkImageMemoryBarrier*) { ++barriers; };
        native.MapMemory=[](VkDevice,VkDeviceMemory,VkDeviceSize,VkDeviceSize,uint32_t,void** data) { *data=staging.data(); return VK_SUCCESS; };
        native.UnmapMemory=[](VkDevice,VkDeviceMemory) {};
        native.DestroyImage=[](VkDevice,VkImage,const VkAllocationCallbacks*) {};
        native.DestroyBuffer=[](VkDevice,VkBuffer,const VkAllocationCallbacks*) {};
        native.FreeMemory=[](VkDevice,VkDeviceMemory,const VkAllocationCallbacks*) {};
        PyroWaveVulkan::Probe owner(nullptr,vk,[](const auto&) {},{}); PyroWaveVulkan::ProbeTestAccess::device(owner);
        bool wrongThread=false;
        std::thread worker([&] { try { owner.draw(); } catch(const std::exception&) { wrongThread=true; } }); worker.join(); require(wrongThread);
        {
            Stage4::Presenter p(owner,vk,native,queue,[](const auto&) {},Stage3::Path::Fragment,"",2560,1440,true);
            Stage4::PresenterTestAccess::seed(p);
            for(unsigned i=0;i<3;++i) {
                require(p.acquire(i).sync.value==1 && p.release(i).sync.value==2);
                p.decoded(i,PyroWave::YuvRange::Full,Stage4::Filter::Linear); p.retire(i);
                require(submissions.back().wait==2 && submissions.back().signal==3 && p.acquire(i).sync.value==3);
                // Queued retirement is NOT completion; the next decode's acquire
                // waits on 3, even though our asynchronous GPU has completed zero.
                require(completed[submissions.back().semaphore]==0);
            }
            p.decoded(0,PyroWave::YuvRange::Limited,Stage4::Filter::Linear);
            submitFailure=true; rejects([&] { p.retire(0); }); submitFailure=false;
            require(p.consumerValue(0)==3); rejects([&] { p.acquire(0); }); // Failed submit cannot free payload.
            p.retire(0); require(p.consumerValue(0)==5);
            for(unsigned n=0;n<1000;++n) {
                const auto prior=p.consumerValue(0); p.select(0,PyroWave::YuvRange::Limited,Stage4::Filter::Linear); p.before(handle<VkCommandBuffer>(88));
                VkSemaphore acquired=handle<VkSemaphore>(91),presented=handle<VkSemaphore>(92);
                VkPipelineStageFlags stage=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.waitSemaphoreCount=submit.signalSemaphoreCount=1;
                submit.pWaitSemaphores=&acquired; submit.pSignalSemaphores=&presented; submit.pWaitDstStageMask=&stage;
                p.submission(submit);
                { Stage3::QueueLock::Guard guard(queue); require(vk.QueueSubmit({},1,&submit,{})==VK_SUCCESS); }
                p.submitted(); require(submissions.back().wait==prior && submissions.back().signal==prior+1);
                require(p.acquire(0).sync.value==prior+1);
            }
            Stage4::PresenterTestAccess::overflow(p); rejects([&] { p.select(0,PyroWave::YuvRange::Full,Stage4::Filter::Linear); });
        }
        {
            PyroWaveVulkan::Overlays overlays(owner,vk,native);
            SDL_Surface* input=SDL_CreateRGBSurfaceWithFormat(0,2,1,32,SDL_PIXELFORMAT_BGRA32); require(input!=nullptr);
            std::unique_ptr<SDL_Surface,decltype(&SDL_FreeSurface)> free(input,SDL_FreeSurface);
            auto* pixels=static_cast<uint32_t*>(input->pixels);
            pixels[0]=SDL_MapRGBA(input->format,1,2,3,127); pixels[1]=SDL_MapRGBA(input->format,4,5,6,255);
            require(overlays.update(0,input,true)); PyroWaveVulkan::OverlayTestAccess::inspect(overlays);
            require(!overlays.update(0,nullptr,true));
            const auto timeline=handle<VkSemaphore>(77);
            for(unsigned i=0;i<3;++i) {
                overlays.update(0,input,true); overlays.before({}); overlays.submitted(timeline,i+1);
                PyroWaveVulkan::OverlayTestAccess::state(overlays,int(i),false);
            }
            require(copies==3 && overlays.uploads==3);
            overlays.update(0,input,true); overlays.before({}); require(overlays.pending() && copies==3);
            completed[timeline]=1; overlays.before({}); overlays.submitted(timeline,4); require(!overlays.pending() && copies==4);
            for(unsigned i=0;i<100;++i) { overlays.before({}); overlays.submitted(timeline,5+i); }
            require(copies==4 && overlays.uploads==4); // Redraws don't re-upload.
            require(overlays.update(0,nullptr,false)); PyroWaveVulkan::OverlayTestAccess::state(overlays,-1,false);
            require(!overlays.update(0,nullptr,false));
            require(!overlays.pending()); PyroWaveVulkan::OverlayTestAccess::blendAndPlacement(overlays);
            PyroWaveVulkan::OverlayTestAccess::replacement(overlays);
            overlays.close(); overlays.close();
        }
        vk.DestroyImageView=[](VkDevice,VkImageView h,const VkAllocationCallbacks*) { destroyTracked(h,"view"); };
        native.DestroyImage=[](VkDevice,VkImage h,const VkAllocationCallbacks*) { destroyTracked(h,"image"); };
        native.DestroyBuffer=[](VkDevice,VkBuffer h,const VkAllocationCallbacks*) { destroyTracked(h,"buffer"); };
        native.FreeMemory=[](VkDevice,VkDeviceMemory h,const VkAllocationCallbacks*) { destroyTracked(h,"memory"); };
        unsigned resources=100;
        for(unsigned prefix=0;prefix<=resources;++prefix) {
            liveResources.clear(); resourceParent.clear();
            PyroWaveVulkan::Overlays overlay(owner,vk,native);
            resources=PyroWaveVulkan::OverlayTestAccess::seedCleanup(overlay,prefix);
            overlay.close(); overlay.close(); require(cleanupOkay && liveResources.empty());
        }
        std::cout<<"PASS overlay partial cleanup: "<<resources+1<<" prefixes, idempotent, views/images/buffers before memory\n";
        require(queue.balanced());
        std::cout<<"PASS REAL production dispatch: GPU-only dropped retirement; failure preserves payload; 1000 monotonic retained redraws; WSI owner thread; RGBA surface conversion; 3 bounded overlay generations; deferred newest update; no redraw uploads; disable/idempotent teardown\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
