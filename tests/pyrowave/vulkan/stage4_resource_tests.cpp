#define SDL_MAIN_HANDLED
#include "stage4_presenter.h"
#include <map>
#include <iostream>
// GPU-free exercise of the REAL close() for every partial initialization prefix.
namespace {
std::map<uintptr_t,std::string> live;
std::map<uintptr_t,uintptr_t> parent;
bool okay=true;
template<class T> void retire(T handle,const char* kind) {
    const auto id=reinterpret_cast<uintptr_t>(handle);
    if(!live.count(id) || live[id]!=kind) { okay=false; return; }
    for(const auto& link:parent) if(link.second==id && live.count(link.first)) okay=false;
    live.erase(id);
}
}
namespace PyroWaveVulkan {
struct ProbeTestAccess { static void device(Probe& p) { p.device=reinterpret_cast<VkDevice>(uintptr_t(0xffff)); } };
}
namespace Stage4 {
struct PresenterTestAccess {
    static unsigned seed(Presenter& p,unsigned prefix) {
        unsigned count=0;
        const auto add=[&](auto& field,const char* kind,uintptr_t dependency=0) {
            const auto id=uintptr_t(++count);
            if(count<=prefix) { field=reinterpret_cast<std::remove_reference_t<decltype(field)>>(id); live[id]=kind; if(dependency) parent[id]=dependency; }
            return id;
        };
        add(p.pool,"pool"); add(p.complete,"fence");
        const auto image=[&](auto& r) { const auto id=add(r.image,"image"); const auto memory=add(r.allocation,"memory"); add(r.view,"view",id); if(count<=prefix) parent[id]=memory; };
        const auto buffer=[&](auto& r) { const auto id=add(r.buffer,"buffer"); const auto memory=add(r.allocation,"memory"); if(count<=prefix) parent[id]=memory; };
        for(auto& slot:p.slots) { add(slot.timeline,"semaphore"); for(auto& r:slot.planes) image(r); }
        for(auto& sampler:p.samplers) add(sampler,"sampler");
        add(p.descriptors,"descriptor-layout"); add(p.descriptorPool,"descriptor-pool");
        add(p.layout,"pipeline-layout"); for(auto& module:p.modules) add(module,"module");
        image(p.target); buffer(p.staging); buffer(p.upload); add(p.verifyPass,"render-pass"); add(p.verifyFramebuffer,"framebuffer",reinterpret_cast<uintptr_t>(p.target.view));
        for(auto& pipeline:p.pipelines) add(pipeline,"pipeline");
        p.gpu.DestroyPipeline=[](VkDevice,VkPipeline h,const VkAllocationCallbacks*) { retire(h,"pipeline"); };
        p.gpu.DestroyDescriptorPool=[](VkDevice,VkDescriptorPool h,const VkAllocationCallbacks*) { retire(h,"descriptor-pool"); };
        p.gpu.DestroyDescriptorSetLayout=[](VkDevice,VkDescriptorSetLayout h,const VkAllocationCallbacks*) { retire(h,"descriptor-layout"); };
        p.gpu.DestroyPipelineLayout=[](VkDevice,VkPipelineLayout h,const VkAllocationCallbacks*) { retire(h,"pipeline-layout"); };
        p.gpu.DestroyShaderModule=[](VkDevice,VkShaderModule h,const VkAllocationCallbacks*) { retire(h,"module"); };
        p.gpu.DestroySampler=[](VkDevice,VkSampler h,const VkAllocationCallbacks*) { retire(h,"sampler"); };
        return count;
    }
};
}
int main() {
    PyroWaveVulkan::Dispatch vk; Stage3::NativeDispatch native; Stage3::QueueLock queue;
    vk.DestroyDevice=[](VkDevice,const VkAllocationCallbacks*) {};
    vk.DestroyImageView=[](VkDevice,VkImageView h,const VkAllocationCallbacks*) { retire(h,"view"); };
    vk.DestroyFramebuffer=[](VkDevice,VkFramebuffer h,const VkAllocationCallbacks*) { retire(h,"framebuffer"); };
    vk.DestroyRenderPass=[](VkDevice,VkRenderPass h,const VkAllocationCallbacks*) { retire(h,"render-pass"); };
    vk.DestroySemaphore=[](VkDevice,VkSemaphore h,const VkAllocationCallbacks*) { retire(h,"semaphore"); };
    vk.DestroyFence=[](VkDevice,VkFence h,const VkAllocationCallbacks*) { retire(h,"fence"); };
    vk.DestroyCommandPool=[](VkDevice,VkCommandPool h,const VkAllocationCallbacks*) { retire(h,"pool"); };
    native.DestroyImage=[](VkDevice,VkImage h,const VkAllocationCallbacks*) { retire(h,"image"); };
    native.DestroyBuffer=[](VkDevice,VkBuffer h,const VkAllocationCallbacks*) { retire(h,"buffer"); };
    native.FreeMemory=[](VkDevice,VkDeviceMemory h,const VkAllocationCallbacks*) { retire(h,"memory"); };
    PyroWaveVulkan::Probe owner(nullptr,vk,[](const auto&) {},{}); PyroWaveVulkan::ProbeTestAccess::device(owner);
    unsigned total=100;
    for(unsigned prefix=0;prefix<=total;++prefix) {
        live.clear(); parent.clear();
        Stage4::Presenter presenter(owner,vk,native,queue,[](const auto&) {},Stage3::Path::Fragment,"");
        total=Stage4::PresenterTestAccess::seed(presenter,prefix);
        presenter.close(); presenter.close();
        if(!okay || !live.empty()) { std::cerr<<"partial unwind failed prefix="<<prefix<<'\n'; return 1; }
    }
    std::cout<<"Stage 4 real cleanup PASS: "<<total+1<<" partial prefixes, idempotent, children before parent allocation\n";
    return 0;
}
