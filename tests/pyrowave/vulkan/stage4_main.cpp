#define SDL_MAIN_HANDLED
#include "stage4_presenter.h"
#include <SDL_vulkan.h>
#include <chrono>
#include <iostream>
#include <memory>
using namespace PyroWaveVulkan;
namespace {
std::string jsonString(const std::string& value) {
    std::string result="\""; for(auto c:value) { if(c=='"' || c=='\\') result+='\\'; if(c=='\n') result+="\\n"; else if(c!='\r') result+=c; } return result+'"';
}
void json(const std::filesystem::path& path,const std::string& data) { std::ofstream out(path); if(!out || !(out<<data)) throw std::runtime_error("cannot write evidence "+path.string()); }
double milliseconds(std::chrono::steady_clock::time_point start) { return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count(); }
}
int main(int argc,char** argv) {
#ifdef _M_ARM64
    const std::string arch="arm64";
#elif defined(__aarch64__)
    const std::string arch="arm64";
#else
    const std::string arch="x64";
#endif
    std::filesystem::path runtime,evidence="stage4-evidence"; std::string expected,fault; bool hidden=false,verifyOnly=false,exercise=false,forceCompute=false,noVsync=false; unsigned seconds=40;
    for(int i=1;i<argc;++i) {
        const std::string argument=argv[i];
        if(argument=="--source-revision") { std::cout<<STAGE4_SOURCE_REVISION<<'\n'; return 0; }
        if(argument=="--runtime" && i+1<argc) runtime=std::filesystem::absolute(argv[++i]);
        else if(argument=="--evidence" && i+1<argc) evidence=std::filesystem::absolute(argv[++i]);
        else if(argument=="--expect-device" && i+1<argc) expected=argv[++i];
        else if(argument=="--fail" && i+1<argc) fault=argv[++i];
        else if(argument=="--seconds" && i+1<argc) { try { seconds=unsigned(std::stoul(argv[++i])); } catch(...) { return 2; } }
        else if(argument=="--hidden") hidden=true;
        else if(argument=="--verify-only") verifyOnly=hidden=true;
        else if(argument=="--exercise") exercise=true;
        else if(argument=="--force-compute") forceCompute=true;
        else if(argument=="--no-vsync") noVsync=true;
        else { std::cerr<<"unknown/incomplete argument "<<argument<<'\n'; return 2; }
    }
    if(runtime.empty() || seconds<30 || seconds>300 || std::filesystem::exists(evidence)) { std::cerr<<"Provide --runtime and fresh --evidence; seconds 30..300\n"; return 2; }
    if(!fault.empty() && fault!="shader-module" && fault!="sampler" && fault!="descriptor-layout" && fault!="descriptor-pool" && fault!="descriptor-sets" && fault!="pipeline-layout" && fault!="pipeline" && fault!="sampled-view" && fault!="offscreen-target" && fault!="swapchain" && fault!="framebuffer" && fault!="present-semaphore") return 2;
    std::filesystem::create_directories(evidence); std::ofstream output(evidence/"presentation.log"),validation(evidence/"validation.log");
    if(!output || !validation) return 2;
    std::vector<std::string> swapchainRecords;
    auto log=[&](const std::string& line) { output<<line<<'\n'; output.flush(); std::cout<<line<<'\n';
        if(line.rfind("VALIDATION ",0)==0) { validation<<line<<'\n'; validation.flush(); }
        if(line.rfind("swapchain_generation=",0)==0) swapchainRecords.push_back(line); };
    log("source_revision=" STAGE4_SOURCE_REVISION " executable_arch="+arch+" scope=OFFLINE_ONLY owner_qualification=PENDING");
    log("PRESENTATION encoded_fixture->PyroWave_GPU->caller_GPU_YUV->BT709_shader->swapchain cpu_yuv_readback=NO external_memory_handles=0 external_semaphore_handles=0 d3d11_resources=0");
    log("TEST_VERIFICATION exact_planes->GPU_upload->same_shader->offscreen_RGB->CPU_RGB_readback rgb_tolerance=1");
    Dispatch vk; Stage3::NativeDispatch native; Stage3::QueueLock queueLock; Stage3::Requirements requirements{native,vk,log};
    std::array<const char*,3> extensions{VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME,VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    std::unique_ptr<Probe> owner; std::unique_ptr<Stage4::Presenter> presenter; PyroWave::Runtime candidate,fixtureEncoder;
    // Stable storage outlives wrapper even on an exception.
    pyrowave_device_create_queue_info queueInfo{}; pyrowave_device_create_info borrowed{};
    SDL_Window* window=nullptr; bool unavailable=true,borrowedMatch=false,shaderPass=false,cleanup=true,injected=false;
    std::string preferred,actual,error; int result=1;
    uint64_t decoded=0; std::array<unsigned,10> fixturePresents{}; unsigned lifecycleSteps=0;
    double decodeMs=0,submitMs=0;
    try {
        if(!candidate.load(runtime)) throw std::runtime_error(candidate.error());
        vk.load(); log("vulkan_loader="+vk.loaderPath);
        SDL_SetMainReady(); if(SDL_Init(SDL_INIT_VIDEO)!=0) throw std::runtime_error(std::string("SDL unavailable: ")+SDL_GetError());
        if(SDL_Vulkan_LoadLibrary(vk.loaderPath.c_str())!=0) throw std::runtime_error(std::string("SDL Vulkan unavailable: ")+SDL_GetError());
        window=SDL_CreateWindow("Asteria Stage 4 OFFLINE",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,960,540,
            SDL_WINDOW_VULKAN|SDL_WINDOW_RESIZABLE|(hidden ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN));
        if(!window) throw std::runtime_error(std::string("SDL window unavailable: ")+SDL_GetError());
        ProbeOptions options; options.minimumApi=VK_API_VERSION_1_2; options.deviceName=expected; options.vsync=!noVsync;
        options.suitable=[&](VkPhysicalDevice physical) { native.loadInstance(vk,owner->instanceHandle()); return requirements.suitable(physical); };
        options.configure=[&](VkDeviceCreateInfo& info) { requirements.configure(info); info.enabledExtensionCount=3; info.ppEnabledExtensionNames=extensions.data(); };
        options.beforeVideo=[&](VkCommandBuffer cmd) { presenter->before(cmd); };
        options.video=[&](VkCommandBuffer cmd,VkRenderPass pass,VkExtent2D extent) { presenter->record(cmd,pass,extent); };
        options.videoSubmit=[&](VkSubmitInfo& info) { presenter->submission(info); };
        options.videoSubmitted=[&] { presenter->submitted(); };
        if(fault=="swapchain" || fault=="framebuffer" || fault=="present-semaphore") options.failAt=fault;
        owner=std::make_unique<Probe>(window,vk,log,options); owner->initialize(); unavailable=false;
        native.loadDevice(vk,owner->deviceHandle());
        queueInfo={owner->queueHandle(),owner->queueFamilyIndex(),0};
        borrowed.GetInstanceProcAddr=vk.GetInstanceProcAddr; borrowed.instance=owner->instanceHandle(); borrowed.physical_device=owner->physicalHandle(); borrowed.device=owner->deviceHandle();
        borrowed.instance_create_info=&owner->instanceCreateInfo(); borrowed.device_create_info=&owner->deviceCreateInfo(); borrowed.queue_info=&queueInfo; borrowed.queue_info_count=1; borrowed.userdata=&queueLock;
        borrowed.queue_lock_callback=[](void* p) { static_cast<Stage3::QueueLock*>(p)->lock(true); };
        borrowed.queue_unlock_callback=[](void* p) { static_cast<Stage3::QueueLock*>(p)->unlock(true); };
        if(!candidate.borrowDevice(borrowed)) throw std::runtime_error(candidate.error()); borrowedMatch=true;
        preferred=candidate.nativePrefersFragment() ? "fragment" : "compute";
        if(!candidate.createDecoder(1920,1080,!forceCompute)) throw std::runtime_error(candidate.error()); actual=candidate.decoderPath();
        log("same_device=YES borrowed_device_match=YES configured_queue_match=YES preferred_decoder_path="+preferred+" actual_decoder_path="+actual);
        presenter=std::make_unique<Stage4::Presenter>(*owner,vk,native,queueLock,log,actual=="fragment" ? Stage3::Path::Fragment : Stage3::Path::Compute,fault);
        presenter->initialize(); presenter->verify(evidence); shaderPass=true;
        if(!verifyOnly) {
            // Stage 3's existing test-setup device: encoding only, NEVER candidate
            // decoding or presentation. Decoder-only caller features omit float16.
            if(!fixtureEncoder.load(runtime) || !fixtureEncoder.createDecoder(1920,1080)) throw std::runtime_error(fixtureEncoder.error());
            log("fixture_device_role=ENCODE_TEST_INPUT_ONLY candidate_device_role=ASTERIA_BORROWED_ONLY");
            struct Fixture { std::vector<uint8_t> encoded; PyroWave::YuvRange range; Presentation::Pattern pattern; };
            std::array<Fixture,10> fixtures;
            for(unsigned i=0;i<fixtures.size();++i) {
                auto& fixture=fixtures[i]; fixture.pattern=Presentation::Pattern(i%5); fixture.range=i<5 ? PyroWave::YuvRange::Full : PyroWave::YuvRange::Limited;
                const auto pixels=Stage4::fixture(fixture.pattern,fixture.range);
                if(!fixtureEncoder.encodeProofPixels(pixels,fixture.encoded)) throw std::runtime_error(fixtureEncoder.error());
                PyroWave::Frame frame; std::string parseError; const PyroWave::StreamContext context{1920,1080,PyroWave::Chroma::Yuv420,true};
                if(!PyroWave::parseFrame(fixture.encoded.data(),fixture.encoded.size(),fixture.encoded.size(),frame,parseError,&context)) throw std::runtime_error(parseError);
                Stage4::metadata(fixture.encoded,frame);
                const auto header=std::find_if(frame.records.begin(),frame.records.end(),[](const auto& r) { return r.kind==PyroWave::RecordKind::Sequence; });
                auto& rangeByte=fixture.encoded.at(header->offset+7); rangeByte=uint8_t((rangeByte&~0x40u)|(i<5 ? 0 : 0x40));
                frame.clear(); if(!PyroWave::parseFrame(fixture.encoded.data(),fixture.encoded.size(),fixture.encoded.size(),frame,parseError,&context)) throw std::runtime_error(parseError);
                fixture.range=Stage4::metadata(fixture.encoded,frame).range;
            }
            const Uint64 start=SDL_GetTicks64(); int lastFixture=-1,lastStep=-1; bool running=true,pending=false;
            unsigned active=0;
            while(running && SDL_GetTicks64()-start<uint64_t(seconds)*1000) {
                const auto elapsed=SDL_GetTicks64()-start;
                if(exercise && !hidden) {
                    const int step=int(elapsed/1000);
                    if(step!=lastStep) { lastStep=step; ++lifecycleSteps;
                        switch(step%8) {
                        case 2: SDL_MinimizeWindow(window); log("lifecycle=minimize"); break;
                        case 3: SDL_RestoreWindow(window); owner->resize(); log("lifecycle=restore"); break;
                        case 4: SDL_MaximizeWindow(window); owner->resize(); log("lifecycle=maximize"); break;
                        case 5: SDL_RestoreWindow(window); owner->resize(); log("lifecycle=restore"); break;
                        default: SDL_SetWindowSize(window,step%2 ? 1001 : 960,step%2 ? 751 : 540); owner->resize(); log("lifecycle=resize/rebuild");
                        }
                    }
                }
                if(!pending) {
                    active=unsigned(elapsed/3000)%10; const auto& fixture=fixtures[active];
                    if(int(active)!=lastFixture) { lastFixture=int(active); const auto title="Asteria Stage 4 OFFLINE | "+std::string(Presentation::name(fixture.pattern))+" | "+(fixture.range==PyroWave::YuvRange::Full ? "FULL" : "LIMITED")+" | "+(fixture.pattern==Presentation::Pattern::Chroma ? "NEAREST CENTER" : "LINEAR CENTER"); SDL_SetWindowTitle(window,title.c_str()); log("presentation_range="+std::string(fixture.range==PyroWave::YuvRange::Full ? "FULL" : "LIMITED")+" pattern="+Presentation::name(fixture.pattern)); }
                    const unsigned slot=unsigned(decoded%3); const auto views=presenter->views(slot); const auto acquire=presenter->acquire(slot),release=presenter->release(slot);
                    const auto began=std::chrono::steady_clock::now();
                    if(!candidate.decodeNative(fixture.encoded,views,acquire,release)) throw std::runtime_error(candidate.error()); decodeMs+=milliseconds(began);
                    presenter->decoded(slot,fixture.range,fixture.pattern==Presentation::Pattern::Chroma ? Stage4::Filter::Nearest : Stage4::Filter::Linear); ++decoded; pending=true;
                }
                const auto began=std::chrono::steady_clock::now();
                { Stage3::QueueLock::Guard lock(queueLock); if(owner->draw()) { ++fixturePresents[active]; pending=false; } }
                submitMs+=milliseconds(began);
                SDL_Event event{}; if(SDL_WaitEventTimeout(&event,Progress::retryMilliseconds)) {
                    if(event.type==SDL_QUIT || (event.type==SDL_KEYDOWN && event.key.keysym.sym==SDLK_ESCAPE)) running=false;
                    if(event.type==SDL_KEYDOWN && event.key.keysym.sym==SDLK_r) owner->resize();
                    if(event.type==SDL_WINDOWEVENT && (event.window.event==SDL_WINDOWEVENT_SIZE_CHANGED || event.window.event==SDL_WINDOWEVENT_RESTORED || event.window.event==SDL_WINDOWEVENT_DISPLAY_CHANGED)) owner->resize();
                }
                if(owner->validationErrors) throw std::runtime_error("Vulkan validation ERROR; STOP");
            }
            for(auto count:fixturePresents) if(!count) throw std::runtime_error("visible sequence incomplete; no owner qualification");
            if(exercise && !hidden && owner->recreations<20) throw std::runtime_error("fewer than 20 swapchain rebuilds");
        }
        result=0;
    } catch(const std::exception& e) {
        error=e.what(); log("error="+error);
        injected=(!fault.empty() && error=="injected Stage 4 "+fault) || (owner && owner->injectedFailure);
        const bool absent=error.find("System32 Vulkan loader unavailable")!=std::string::npos || error.find("no non-software physical device")!=std::string::npos || error.find("no suitable Stage 3 loader API")!=std::string::npos;
        result=unavailable && absent ? 77 : 1;
    }
    // Final drain is allowed; no application queue/device idle in normal frames.
    if(owner && owner->deviceHandle()) {
        Stage3::QueueLock::Guard lock(queueLock); const auto status=vk.DeviceWaitIdle(owner->deviceHandle()); cleanup=status==VK_SUCCESS; log("final_drain="+std::to_string(status));
    }
    fixtureEncoder.close();
    candidate.close(); // Decoder/wrapper before caller views/images; info still alive.
    if(presenter) presenter->close();
    if(owner) { Stage3::QueueLock::Guard lock(queueLock); owner->close(); cleanup=cleanup && owner->cleanupOkay; }
    const unsigned errors=owner ? owner->validationErrors.load() : 0,warnings=owner ? owner->validationWarnings.load() : 0;
    const std::string validationStatus=owner && owner->validationActive ? (errors ? "FAIL" : "NO_REPORTED_ERRORS") : "SKIP";
    if(!cleanup || !queueLock.balanced() || errors) result=1;
    else if(injected) result=0;
    if(window) SDL_DestroyWindow(window); SDL_Vulkan_UnloadLibrary(); SDL_Quit();
    try {
        json(evidence/"timings.json","{\"units\":\"CPU-observed milliseconds; no GPU/end-to-end latency claim\",\"decodeApiTotal\":"+std::to_string(decodeMs)+",\"queueSubmitTotal\":"+std::to_string(owner ? owner->queueSubmitMs : 0)+",\"presentCallTotal\":"+std::to_string(owner ? owner->presentCallMs : 0)+",\"presenterLoopTotal\":"+std::to_string(submitMs)+",\"decodedFrames\":"+std::to_string(decoded)+",\"presentCalls\":"+std::to_string(owner ? owner->presents : 0)+"}");
        json(evidence/"validation.json","{\"status\":"+jsonString(validationStatus)+",\"errors\":"+std::to_string(errors)+",\"warnings\":"+std::to_string(warnings)+"}");
        json(evidence/"lifecycle.json","{\"cleanupPass\":"+std::string(cleanup ? "true" : "false")+",\"queueLockBalanced\":"+(queueLock.balanced() ? "true" : "false")+",\"automaticSteps\":"+std::to_string(lifecycleSteps)+",\"recreations\":"+std::to_string(owner ? owner->recreations : 0)+",\"hidden\":"+(hidden ? "true" : "false")+"}");
        std::string records="["; for(const auto& record:swapchainRecords) { if(records.size()>1) records+=','; records+=jsonString(record); } records+=']';
        json(evidence/"swapchain.json","{\"formatContract\":\"SDR encoded RGB to UNORM / SRGB_NONLINEAR\",\"generations\":"+records+"}");
        std::string counts="["; for(auto count:fixturePresents) { if(counts.size()>1) counts+=','; counts+=std::to_string(count); } counts+=']';
        const bool api=result==0 && !injected;
        json(evidence/"api-result.json","{\"result\":"+jsonString(result==77 ? "SKIP" : (result ? "FAIL" : (injected ? "FAULT_CLEANUP_PASS" : "API_PASS")))+",\"sourceRevision\":\"" STAGE4_SOURCE_REVISION "\",\"architecture\":"+jsonString(arch)+",\"selectedDevice\":"+jsonString(owner ? owner->selectedDevice : "")+",\"borrowedDeviceMatch\":"+(borrowedMatch ? "true" : "false")+",\"preferredDecoderPath\":"+jsonString(preferred)+",\"actualDecoderPath\":"+jsonString(actual)+",\"presentationBackend\":\"raw Vulkan\",\"presentationPath\":\"GPU_DECODE_CALLER_YUV_SHADER_SWAPCHAIN\",\"cpuYuvReadbackInPresentation\":false,\"externalMemoryHandles\":0,\"externalSemaphoreHandles\":0,\"d3d11Resources\":0,\"shaderVerificationPass\":"+(shaderPass ? "true" : "false")+",\"chromaSitingPass\":"+(shaderPass ? "true" : "false")+",\"maxRgbError\":"+std::to_string(presenter ? presenter->maxRgbError : 0)+",\"fixturePresents\":"+counts+",\"decodedFrames\":"+std::to_string(decoded)+",\"swapchainRecreationCount\":"+std::to_string(owner ? owner->recreations : 0)+",\"validationStatus\":"+jsonString(validationStatus)+",\"validationErrors\":"+std::to_string(errors)+",\"validationWarnings\":"+std::to_string(warnings)+",\"ownerVisualConfirmed\":false,\"stage4Qualification\":\"OWNER_QUALIFICATION_PENDING\",\"verifyOnly\":"+(verifyOnly ? "true" : "false")+",\"apiPass\":"+(api ? "true" : "false")+",\"decodeApiTotalMs\":"+std::to_string(decodeMs)+",\"presenterCpuTotalMs\":"+std::to_string(submitMs)+",\"error\":"+jsonString(error)+"}");
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 2; }
    return result;
}
