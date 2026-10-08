#include "pyrowave_vulkan_live.h"
#include "pyrowave_vulkan_overlays.h"
#include <chrono>
#include <SDL_syswm.h>
#include <mutex>

namespace PyroWave {
namespace {
uint64_t micros() { return uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()); }
}
struct NativePresentation::Impl {
    NativePresentation& self;
    Runtime& runtime;
    DWORD thread=GetCurrentThreadId();
    SDL_Window* window=nullptr;
    std::mutex mutex; // State/data, separate from the nonrecursive VkQueue lock.
    std::string error;
    bool stopped=false,initialized=false,overlayChanged=false;
    PyroWaveVulkan::Dispatch vk;
    Stage3::NativeDispatch native;
    Stage3::QueueLock queue;
    PyroWaveVulkan::Log log=[](const std::string& message) { SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,"PyroWave native: %s",message.c_str()); };
    Stage3::Requirements requirements{native,vk,log};
    std::array<const char*,3> extensions{VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME,VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    // These never move and outlive borrowed wrapper destruction.
    pyrowave_device_create_queue_info queueInfo{};
    pyrowave_device_create_info borrowed{};
    std::unique_ptr<PyroWaveVulkan::Probe> owner;
    std::unique_ptr<Stage4::Presenter> presenter;
    std::unique_ptr<PyroWaveVulkan::Overlays> overlays;
    PyroWaveVulkan::LiveSlots slots;
    std::array<uint64_t,3> readyUs{},decodeCounts{};
    int selected=-1;
    explicit Impl(NativePresentation& s,Runtime& r):self(s),runtime(r) {}
    void mainThread() const { if(thread!=GetCurrentThreadId()) throw std::runtime_error("native presentation outside SDL owner thread"); }
    void retire(int slot) {
        if(presenter->retire(unsigned(slot))) {
            ++self.diagnostics.retiredDrops;
            ++self.diagnostics.decodeWaits; ++self.diagnostics.consumerSignals;
        }
        ++self.diagnostics.presentationDrops;
    }
};
NativePresentation::NativePresentation(Runtime& runtime):impl(new Impl(*this,runtime)) {}
NativePresentation::~NativePresentation() { shutdown(); }
uint64_t NativePresentation::presentationDrops() const { std::lock_guard<std::mutex> guard(impl->mutex); return diagnostics.presentationDrops; }
const std::string& NativePresentation::error() const { return impl->error; }
void NativePresentation::fatal(const std::string& reason) {
    auto& p=*impl; std::lock_guard<std::mutex> guard(p.mutex);
    if(p.error.empty()) {
        p.error=reason; ++diagnostics.fatalErrors;
        if(reason.find("timeline")!=std::string::npos || reason.find("payload")!=std::string::npos || reason.find("sampling without decode")!=std::string::npos)
            ++diagnostics.timelineErrors;
    }
    p.stopped=true;
}
bool NativePresentation::initialize(SDL_Window* window,int width,int height,bool vsync) {
    auto& p=*impl; p.mainThread(); p.window=window;
    try {
        // Runtime is verified/loaded by the decoder. Never create its default device.
        p.vk.load(); p.log("vulkan_loader="+p.vk.loaderPath);
        PyroWaveVulkan::ProbeOptions options; options.minimumApi=VK_API_VERSION_1_2;
        options.existingWin32Window=true; options.vsync=vsync;
        options.lockQueue=[&p] { p.queue.lock(false); };
        options.unlockQueue=[&p] { p.queue.unlock(false); };
        options.suitable=[&p](VkPhysicalDevice physical) { p.native.loadInstance(p.vk,p.owner->instanceHandle()); return p.requirements.suitable(physical); };
        options.configure=[&p](VkDeviceCreateInfo& info) { p.requirements.configure(info); info.enabledExtensionCount=3; info.ppEnabledExtensionNames=p.extensions.data(); };
        options.beforeVideo=[&p](VkCommandBuffer cmd) {
            // Begin a retained redraw only after WSI has acquired an image.
            // A retry before recording must not leave the displayed payload
            // pending when a newer decoded frame supersedes it.
            if(p.presenter) {
                if(p.selected>=0) p.presenter->select(unsigned(p.selected),*p.runtime.liveRange(),Stage4::Filter::Linear);
                p.presenter->before(cmd);
            }
            if(p.overlays) p.overlays->before(cmd);
        };
        options.video=[&p](VkCommandBuffer cmd,VkRenderPass pass,VkExtent2D extent) { if(p.presenter) p.presenter->record(cmd,pass,extent); if(p.overlays) p.overlays->record(cmd,pass,extent); };
        options.videoSubmit=[&p](VkSubmitInfo& submit) { if(p.presenter) p.presenter->submission(submit); };
        options.videoSubmitted=[&p] {
            if(!p.presenter) return;
            p.presenter->submitted();
            if(p.selected>=0) {
                const unsigned i=unsigned(p.selected);
                ++p.self.diagnostics.decodeWaits; ++p.self.diagnostics.consumerSignals;
                if(p.overlays) p.overlays->submitted(p.presenter->slotTimeline(i),p.presenter->consumerValue(i));
            }
        };
        p.owner=std::make_unique<PyroWaveVulkan::Probe>(window,p.vk,p.log,options); p.owner->initialize();
        p.native.loadDevice(p.vk,p.owner->deviceHandle());
        p.queueInfo={p.owner->queueHandle(),p.owner->queueFamilyIndex(),0};
        p.borrowed.GetInstanceProcAddr=p.vk.GetInstanceProcAddr; p.borrowed.instance=p.owner->instanceHandle();
        p.borrowed.physical_device=p.owner->physicalHandle(); p.borrowed.device=p.owner->deviceHandle();
        p.borrowed.instance_create_info=&p.owner->instanceCreateInfo(); p.borrowed.device_create_info=&p.owner->deviceCreateInfo();
        p.borrowed.queue_info=&p.queueInfo; p.borrowed.queue_info_count=1; p.borrowed.userdata=&p.queue;
        p.borrowed.queue_lock_callback=[](void* q) { static_cast<Stage3::QueueLock*>(q)->lock(true); };
        p.borrowed.queue_unlock_callback=[](void* q) { static_cast<Stage3::QueueLock*>(q)->unlock(true); };
        if(!p.runtime.borrowDevice(p.borrowed)) throw std::runtime_error(p.runtime.error());
        diagnostics.borrowedMatch=true; diagnostics.preferredPath=p.runtime.nativePrefersFragment() ? "fragment" : "compute";
        if(!p.runtime.createDecoder(width,height,true)) throw std::runtime_error(p.runtime.error());
        diagnostics.actualPath=p.runtime.decoderPath(); diagnostics.gpu=p.owner->selectedDevice;
        p.presenter=std::make_unique<Stage4::Presenter>(*p.owner,p.vk,p.native,p.queue,p.log,
            diagnostics.actualPath=="fragment" ? Stage3::Path::Fragment : Stage3::Path::Compute,"",width,height,true);
        p.presenter->initialize();
        p.overlays=std::make_unique<PyroWaveVulkan::Overlays>(*p.owner,p.vk,p.native); p.overlays->initialize();
        // Bounded startup black swapchain frame; both video/overlay pipelines are
        // built, with no sampling of undecoded planes and no video CPU output.
        bool presented=false;
        for(unsigned attempt=0;attempt<100 && !presented;++attempt) { presented=p.owner->draw(); if(!presented) SDL_Delay(16); }
        if(!presented) throw std::runtime_error("native harmless black preflight presentation unavailable");
        if(p.owner->validationErrors) throw std::runtime_error("native preflight validation ERROR");
        p.owner->queueSubmitMs=p.owner->presentCallMs=0; // Live CPU intervals exclude startup.
        p.initialized=true;
        p.log("backend=NATIVE_VULKAN presentationPath=GPU_DECODE_CALLER_YUV_SHADER_SWAPCHAIN cpu_yuv_readback=NO externalMemoryHandles=0 externalSemaphoreHandles=0 d3d11Resources=0 slots=3");
        p.log("borrowedInstanceMatch=true borrowedPhysicalDeviceMatch=true borrowedDeviceMatch=true preferredDecoderPath="+diagnostics.preferredPath+" actualDecoderPath="+diagnostics.actualPath);
        return true;
    } catch(const std::exception& e) { p.error=e.what(); shutdown(); return false; }
}
bool NativePresentation::decode(const std::vector<uint8_t>& bytes,size_t& packets,DecodeTiming& timing) {
    auto& p=*impl;
    // This mutex serializes timeline/publication state, never nests a codec queue
    // callback lock. Main/decoder may submit through that separate queue mutex.
    std::lock_guard<std::mutex> guard(p.mutex);
    if(p.stopped || !p.initialized) throw std::runtime_error("native decode after shutdown/fatal failure");
    const int slot=p.slots.reserve([&p](int i) { p.retire(i); });
    const auto acquire=p.presenter->acquire(unsigned(slot)),release=p.presenter->release(unsigned(slot));
    if(!p.runtime.decodeLiveNative(bytes,p.presenter->views(unsigned(slot)),acquire,release,packets,timing)) {
        if(p.runtime.frameRejected()) { p.slots.cancel(slot); return false; }
        throw std::runtime_error(p.runtime.error()); // No slot reuse or fallback after GPU failure.
    }
    if(p.decodeCounts[slot]++) ++diagnostics.slotReuse;
    p.presenter->decoded(unsigned(slot),*p.runtime.liveRange(),Stage4::Filter::Linear);
    p.readyUs[slot]=micros();
    p.slots.publish(slot,[&p](int i) { p.retire(i); });
    ++diagnostics.decoded; diagnostics.decodeSubmitUs+=timing.decodeUs;
    return true;
}
NativePresentation::RenderResult NativePresentation::render() {
    auto& p=*impl; p.mainThread(); std::lock_guard<std::mutex> guard(p.mutex);
    if(p.stopped) throw std::runtime_error(p.error.empty() ? "native presenter stopped" : p.error);
    RenderResult result;
    if(SDL_GetWindowFlags(p.window)&SDL_WINDOW_MINIMIZED) { p.slots.suspend([&p](int i) { p.retire(i); }); return result; }
    SDL_SysWMinfo wm{}; SDL_VERSION(&wm.version);
    if(!SDL_GetWindowWMInfo(p.window,&wm)) throw std::runtime_error(SDL_GetError());
    RECT rect{}; if(!GetClientRect(wm.info.win.window,&rect)) throw std::runtime_error("drawable HWND unavailable");
    if(rect.right<=rect.left || rect.bottom<=rect.top) { p.slots.suspend([&p](int i) { p.retire(i); }); return result; }
    p.selected=p.slots.current(); if(p.selected<0) return result;
    const bool newFrame=p.slots.pending>=0;
    const bool overlayWork=p.overlayChanged || p.overlays->pending();
    const auto started=micros();
    if(p.owner->draw()) {
        if(newFrame) { result.newFrame=true; result.readyUs=micros()-p.readyUs[p.selected]; ++diagnostics.rendered; }
        else {
            ++diagnostics.retainedFrameRedraws;
            if(overlayWork) ++diagnostics.overlayRedraws;
        }
        p.overlayChanged=false;
        p.slots.presented(p.selected);
        result.retry=p.overlays->pending();
    } else result.retry=true; // Serviced by a bounded SDL timer, never busy reposting.
    diagnostics.renderLoopUs+=micros()-started;
    diagnostics.recreations=p.owner->recreations; diagnostics.overlayUploads=p.overlays->uploads;
    diagnostics.queueSubmitUs=uint64_t(p.owner->queueSubmitMs*1000); diagnostics.presentCallUs=uint64_t(p.owner->presentCallMs*1000);
    if(p.owner->validationErrors) throw std::runtime_error("native presentation validation ERROR");
    return result;
}
void NativePresentation::updateOverlay(unsigned index,SDL_Surface* surface,bool enabled) {
    auto& p=*impl; p.mainThread(); std::lock_guard<std::mutex> guard(p.mutex);
    p.overlayChanged=p.overlays->update(index,surface,enabled) || p.overlayChanged;
}
void NativePresentation::windowChanged() { auto& p=*impl; p.mainThread(); p.owner->resize(); }
void NativePresentation::shutdown() noexcept {
    auto& p=*impl; if(p.thread!=GetCurrentThreadId()) std::terminate();
    std::lock_guard<std::mutex> guard(p.mutex); if(p.stopped && !p.owner) return;
    p.stopped=true;
    if(p.owner && p.owner->deviceHandle()) {
        try {
            if(p.presenter) p.slots.suspend([&p](int i) { p.retire(i); });
            Stage3::QueueLock::Guard queue(p.queue);
            const auto result=p.vk.DeviceWaitIdle(p.owner->deviceHandle());
            if(result!=VK_SUCCESS) { diagnostics.cleanupOkay=false; if(p.initialized) ++diagnostics.fatalErrors; }
        } catch(const std::exception& e) { diagnostics.cleanupOkay=false; p.log(std::string("native teardown: ")+e.what()); }
    }
    // Consumer work is drained before codec/wrapper destruction. The wrapper
    // borrows every owner handle and all callback/create-info storage until close.
    p.runtime.resetDecoder();
    if(diagnostics.decoded) p.runtime.reportPerformanceStats([](void*,const char* text) {
        if(text && *text) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,"PyroWave GPU timing: %s",text);
    },nullptr);
    diagnostics.cpuReadbacks=p.runtime.cpuYuvReadbackFrames();
    p.runtime.close();
    if(p.overlays) { diagnostics.overlayUploads=p.overlays->uploads; p.overlays.reset(); }
    p.presenter.reset();
    if(p.owner) {
        diagnostics.recreations=p.owner->recreations; diagnostics.validationActive=p.owner->validationActive;
        p.owner->close(); // Include destruction-time validation in the lifetime summary.
        diagnostics.validationErrors=p.owner->validationErrors; diagnostics.validationWarnings=p.owner->validationWarnings;
        diagnostics.cleanupOkay=diagnostics.cleanupOkay && p.owner->cleanupOkay; p.owner.reset();
    }
    diagnostics.cleanupOkay=diagnostics.cleanupOkay && p.queue.balanced();
}
}
