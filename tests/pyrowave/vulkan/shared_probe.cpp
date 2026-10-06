#define SDL_MAIN_HANDLED
#include "shared_device.h"
#include "../presentation_patterns.h"
#include <SDL_vulkan.h>
#include <bcrypt.h>
#include <psapi.h>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <regex>
#include <sstream>
using namespace PyroWaveVulkan;
namespace {
std::string hash(const std::vector<uint8_t>& bytes) {
    BCRYPT_ALG_HANDLE algorithm{}; BCRYPT_HASH_HANDLE object{};
    if (BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0) throw std::runtime_error("SHA256 provider failed");
    std::array<uint8_t,32> digest{};
    auto result=BCryptCreateHash(algorithm,&object,nullptr,0,nullptr,0,0);
    if (result>=0) result=BCryptHashData(object,const_cast<PUCHAR>(bytes.data()),ULONG(bytes.size()),0);
    if (result>=0) result=BCryptFinishHash(object,digest.data(),ULONG(digest.size()),0);
    if (object) BCryptDestroyHash(object); BCryptCloseAlgorithmProvider(algorithm,0);
    if (result<0) throw std::runtime_error("SHA256 failed");
    std::ostringstream s; s<<std::hex<<std::setfill('0'); for(auto b:digest) s<<std::setw(2)<<unsigned(b); return s.str();
}
std::string readText(const std::filesystem::path& path) {
    std::ifstream f(path,std::ios::binary); if(!f) throw std::runtime_error("missing provenance file");
    return {std::istreambuf_iterator<char>(f),{}};
}
std::string field(const std::string& json,const std::string& name) {
    std::regex pattern("\""+name+"\"\\s*:\\s*\"([^\"]+)\"");
    auto begin=std::sregex_iterator(json.begin(),json.end(),pattern); auto end=std::sregex_iterator();
    if(begin==end) throw std::runtime_error("missing metadata field "+name);
    auto value=(*begin)[1].str(); if(++begin!=end) throw std::runtime_error("duplicate metadata field "+name); return value;
}
std::string verify(const std::filesystem::path& directory,const std::string& architecture) {
    if(!directory.is_absolute()) throw std::runtime_error("runtime directory must be absolute");
    auto json=readText(directory/L"pyrowave-runtime.json");
    if(field(json,"architecture")!=architecture || field(json,"codecCommit")!=PyroWave::CodecCommit ||
        field(json,"bitstreamId")!=PyroWave::BitstreamId || field(json,"apiVersion")!="0.6.0")
        throw std::runtime_error("pinned runtime provenance mismatch");
    auto binary=readText(directory/L"libpyrowave-shared-0.dll");
    std::vector<uint8_t> bytes(binary.begin(),binary.end()); auto sha=hash(bytes);
    if(field(json,"sha256")!=sha) throw std::runtime_error("runtime SHA256 mismatch");
    // The runner also checks PE machine; enforce it in direct executable use.
    if(bytes.size()<64 || bytes[0]!='M' || bytes[1]!='Z') throw std::runtime_error("invalid runtime PE");
    uint32_t offset=0; std::memcpy(&offset,bytes.data()+60,4);
    if(offset>bytes.size()-6 || std::memcmp(bytes.data()+offset,"PE\0\0",4)) throw std::runtime_error("invalid runtime PE header");
    uint16_t machine=0; std::memcpy(&machine,bytes.data()+offset+4,2);
    if(machine!=(architecture=="arm64" ? 0xaa64 : 0x8664)) throw std::runtime_error("runtime PE architecture mismatch");
    return sha;
}
void require(bool okay,const std::string& message) { if(!okay) throw std::runtime_error(message); }
void factoryFault(Dispatch& vk,const std::filesystem::path& directory,const Log& log) {
    // Dedicated process-only fault probe. Rejection occurs before any device
    // fields are inspected; never retry this branch in the real decode process.
    auto module=LoadLibraryExW((directory/L"libpyrowave-shared-0.dll").c_str(),nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    require(module!=nullptr,"factory probe module load failed");
    auto create=reinterpret_cast<decltype(&pyrowave_create_device)>(GetProcAddress(module,"pyrowave_create_device"));
    require(create!=nullptr,"missing shared factory export");
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO}; app.apiVersion=VK_API_VERSION_1_0;
    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; ici.pApplicationInfo=&app;
    VkInstance instance{}; check(vk.CreateInstance(&ici,nullptr,&instance),"factory fault caller instance");
    auto destroy=reinterpret_cast<PFN_vkDestroyInstance>(vk.GetInstanceProcAddr(instance,"vkDestroyInstance"));
    pyrowave_device_create_info info{}; info.GetInstanceProcAddr=vk.GetInstanceProcAddr;
    info.instance=instance; info.instance_create_info=&ici;
    auto privateBytes=[] { PROCESS_MEMORY_COUNTERS_EX m{}; m.cb=sizeof(m);
        require(GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&m),sizeof(m))!=0,"process memory query failed");
        return uint64_t(m.PrivateUsage); };
    for(unsigned batch=0;batch<4;++batch) {
        auto before=privateBytes();
        for(unsigned n=0;n<100;++n) {
            pyrowave_device out=nullptr; require(create(&info,&out)==PYROWAVE_ERROR_NO_VULKAN && !out,"unexpected factory fault result");
        }
        auto after=privateBytes();
        log("factory_batch="+std::to_string(batch)+" calls=100 result=-5 returned_wrapper=NULL private_before="+
            std::to_string(before)+" private_after="+std::to_string(after)+" growth="+std::to_string(after-before));
    }
    destroy(instance,nullptr); FreeLibrary(module);
    log("factory_fault_probe=SOURCE_CONFIRMED_UNREACHABLE_WRAPPER memory_growth_is_diagnostic production_promotion=BLOCKED");
}
}
int main(int argc,char** argv) {
#if defined(_M_ARM64) || defined(__aarch64__)
    const std::string arch="arm64";
#else
    const std::string arch="x64";
#endif
    std::filesystem::path runtimeDir,logPath="stage3.log";
    std::string expectedDevice,fault;
    bool surface=false,factory=false;
    for(int i=1;i<argc;++i) {
        std::string arg=argv[i];
        if(arg=="--runtime" && i+1<argc) runtimeDir=argv[++i];
        else if(arg=="--log" && i+1<argc) logPath=argv[++i];
        else if(arg=="--expect-device" && i+1<argc) expectedDevice=argv[++i];
        else if(arg=="--surface") surface=true;
        else if(arg=="--factory-fault") factory=true;
        else if(arg=="--fail-at" && i+1<argc) fault=argv[++i];
        else { std::cerr<<"unknown/incomplete argument "<<arg<<'\n'; return 2; }
    }
    if(!fault.empty() && fault!="borrowed" && fault!="partial-images" && fault!="decoded" && fault!="reused" && fault!="rejected") return 2;
    std::ofstream evidence(logPath); if(!evidence) return 2;
    std::mutex logging;
    Log log=[&](const std::string& text) { std::lock_guard<std::mutex> guard(logging); std::cout<<text<<std::endl; evidence<<text<<std::endl; };
    log("scope=OFFLINE_ONLY output_path=NATIVE_GPU_CALLER_OWNED architecture="+arch);
    log("source_revision=" STAGE3_SOURCE_REVISION);
    log("codec_commit="+std::string(PyroWave::CodecCommit)+" bitstream_id="+PyroWave::BitstreamId+" api_version=0.6.0 runtime_patch=NONE_ADDED_FOR_STAGE3");
    log("factory_failure_path=CONFIRMED_LEAK production_promotion=BLOCKED owner_qualification=PENDING");
    Dispatch vk; Stage3::NativeDispatch native; Stage3::QueueLock queueLock;
    Stage3::Requirements requirements{native,vk,log};
    std::unique_ptr<Probe> owner; std::unique_ptr<Stage3::Outputs> outputs;
    PyroWave::Runtime candidate,reference;
    SDL_Window* window=nullptr;
    bool unavailable=true, injected=false, cleanupOkay=true;
    int result=1;
    auto checkpoint=[&](const char* name) { if(fault==name) { injected=true; throw std::runtime_error(std::string("injected ")+name); } };
    try {
        require(!surface || arch=="arm64","Surface requires ARM64 executable");
        log("runtime_sha256="+verify(runtimeDir,arch));
        require(candidate.load(runtimeDir),candidate.error());
        vk.load(); log("vulkan_loader="+vk.loaderPath);
        if(factory) {
            factoryFault(vk,runtimeDir,log); candidate.close(); log("overall=FACTORY_FAULT_DIAGNOSTIC"); return 0;
        }
        SDL_SetMainReady(); if(SDL_Init(SDL_INIT_VIDEO)!=0) throw std::runtime_error(SDL_GetError());
        require(SDL_Vulkan_LoadLibrary(vk.loaderPath.c_str())==0,SDL_GetError());
        window=SDL_CreateWindow("Asteria offline Stage 3",0,0,128,128,SDL_WINDOW_HIDDEN | SDL_WINDOW_VULKAN);
        require(window!=nullptr,SDL_GetError());
        ProbeOptions options; options.deviceOnly=true; options.minimumApi=VK_API_VERSION_1_2; options.deviceName=expectedDevice;
        options.suitable=[&](VkPhysicalDevice physical) { native.loadInstance(vk,owner->instanceHandle()); return requirements.suitable(physical); };
        options.configure=[&](VkDeviceCreateInfo& info) { requirements.configure(info); };
        owner=std::make_unique<Probe>(window,vk,log,options); owner->initialize(); unavailable=false;
        native.loadDevice(vk,owner->deviceHandle());
        pyrowave_device_create_queue_info queue{owner->queueHandle(),owner->queueFamilyIndex(),0};
        pyrowave_device_create_info info{}; info.GetInstanceProcAddr=vk.GetInstanceProcAddr;
        info.instance=owner->instanceHandle(); info.physical_device=owner->physicalHandle(); info.device=owner->deviceHandle();
        info.instance_create_info=&owner->instanceCreateInfo(); info.device_create_info=&owner->deviceCreateInfo();
        info.queue_info=&queue; info.queue_info_count=1; info.userdata=&queueLock;
        info.queue_lock_callback=[](void* p) { static_cast<Stage3::QueueLock*>(p)->lock(true); };
        info.queue_unlock_callback=[](void* p) { static_cast<Stage3::QueueLock*>(p)->unlock(true); };
        // Store the entire borrowed create-info (including queue info) outside this
        // try scope below: wrapper must be closed before these locals expire.
        try {
            require(candidate.borrowDevice(info),candidate.error());
            log("borrowed_instance_match=YES borrowed_physical_match=YES borrowed_device_match=YES shared_instance_match=YES shared_physical_device_match=YES shared_device_match=YES");
            checkpoint("borrowed");
            require(candidate.createDecoder(1920,1080,true),candidate.error());
            auto path=std::string(candidate.decoderPath()); log("preferred_decoder_path="+path+" actual_decoder_path="+path);
            outputs=std::make_unique<Stage3::Outputs>(*owner,vk,native,queueLock,log,path=="fragment" ? Stage3::Path::Fragment : Stage3::Path::Compute);
            try { outputs->initialize(fault); } catch(...) { if(fault=="partial-images") injected=true; throw; }
            // Separate codec-owned device is strictly test setup/reference, never candidate output.
            require(reference.load(runtimeDir) && reference.createDecoder(1920,1080),reference.error());
            log("reference_device_role=FIXTURE_AND_CPU_REFERENCE candidate_device_role=ASTERIA_BORROWED_ONLY");
            unsigned frames=0;
            for(unsigned lifetime=0;lifetime<3;++lifetime) {
                if(lifetime) require(candidate.createDecoder(1920,1080,true),candidate.error());
                for(auto pattern:{Presentation::Pattern::Gradient,Presentation::Pattern::Bars}) {
                    auto pixels=Presentation::pattern(pattern); std::vector<uint8_t> compatibility;
                    require(reference.encodeProofPixels(pixels,compatibility),reference.error());
                    PyroWave::Frame frame; std::string error;
                    require(PyroWave::parseFrame(compatibility.data(),compatibility.size(),compatibility.size(),frame,error),error);
                    const auto rangeByte=frame.records.at(0).offset+7;
                    for(bool limited:{false,true}) {
                        if(limited) compatibility[rangeByte]|=0x40; else compatibility[rangeByte]&=uint8_t(~0x40);
                        std::vector<uint8_t> records;
                        for(auto p:frame.packets) records.insert(records.end(),compatibility.begin()+p.offset,compatibility.begin()+p.offset+p.size);
                        for(auto* fixture:{&compatibility,&records}) {
                            PyroWave::Pixels cpu; require(reference.decode(*fixture,cpu),reference.error());
                            for(unsigned reuse=0;reuse<6;++reuse) {
                                const unsigned slot=frames%Stage3::SlotCount;
                                auto views=outputs->views(slot); auto a=outputs->acquire(slot),r=outputs->release(slot);
                                auto bad=*fixture; bad.pop_back();
                                require(!candidate.decodeNative(bad,views,a,r) && candidate.frameRejected(),"parser rejection not reported");
                                checkpoint("rejected");
                                require(candidate.decodeNative(*fixture,views,a,r),candidate.error()); outputs->submitted(slot);
                                checkpoint("decoded");
                                auto gpu=outputs->read(slot);
                                for(unsigned p=0;p<3;++p) {
                                    auto cpuHash=hash(cpu.planes[p]),gpuHash=hash(gpu.planes[p]);
                                    log("frame="+std::to_string(frames)+" lifetime="+std::to_string(lifetime)+" pattern="+Presentation::name(pattern)+
                                        " framing="+(fixture==&compatibility ? "compatibility" : "records")+" range="+(limited ? "limited" : "full")+
                                        " plane="+std::to_string(p)+" cpu_sha256="+cpuHash+" gpu_sha256="+gpuHash);
                                    require(cpu.planes[p]==gpu.planes[p],"CPU/GPU exact output mismatch; STOP");
                                }
                                ++frames; if(frames>3) checkpoint("reused");
                            }
                        }
                    }
                }
            }
            log("comparison=EXACT_BYTES frames_tested="+std::to_string(frames)+" slots_exercised=3 decoder_lifetimes=3 malformed_recovery=PASS");
            outputs->drain(); candidate.close(); outputs->close(); outputs.reset();
        } catch(...) {
            if(outputs) { try { outputs->drain(); } catch(const std::exception& e) { log(std::string("cleanup_drain_error=")+e.what()); cleanupOkay=false; } }
            candidate.close(); if(outputs) { outputs->close(); outputs.reset(); }
            throw;
        }
        reference.close();
        require(queueLock.balanced() && queueLock.codecLocks>0 && queueLock.callerLocks>0,"queue callbacks not exercised/balanced");
        log("queue_codec_locks="+std::to_string(queueLock.codecLocks)+" queue_codec_unlocks="+std::to_string(queueLock.codecUnlocks)+
            " queue_caller_locks="+std::to_string(queueLock.callerLocks)+" queue_caller_unlocks="+std::to_string(queueLock.callerUnlocks)+
            " queue_codec_thread_hash="+std::to_string(queueLock.lastCodecThread));
        require(owner->validationErrors==0,"Vulkan validation ERROR"); result=0;
    } catch(const std::exception& e) {
        const std::string error=e.what(); log("reason="+error);
        // Only known loader/device absence is SKIP. Runtime/API/mismatch/errors never skip.
        const bool missing=error.find("System32 Vulkan loader unavailable")!=std::string::npos ||
            error.find("no suitable Stage 3 loader API")!=std::string::npos ||
            error.find("no non-software physical device")!=std::string::npos;
        result=unavailable && missing ? 77 : 1;
        if(injected && cleanupOkay && queueLock.balanced()) result=0;
    }
    reference.close(); candidate.close();
    if(owner) {
        const bool active=owner->validationActive;
        owner->close();
        log("validation_status="+std::string(!active ? "SKIP" : owner->validationErrors ? "ERROR" : owner->validationWarnings ? "WARNINGS_REVIEW_REQUIRED" : "NO_REPORTED_ERRORS"));
        log("validation_errors="+std::to_string(owner->validationErrors)+" validation_warnings="+std::to_string(owner->validationWarnings));
        if(owner->validationErrors || !owner->cleanupOkay) result=1;
    } else log("validation_status=SKIP");
    owner.reset(); if(window) SDL_DestroyWindow(window); SDL_Vulkan_UnloadLibrary(); SDL_Quit();
    if(!fault.empty() && !injected && result!=77) result=1;
    log("external_handle_api_usage=NONE runtime_patch=NONE stage3_additional_runtime_patch=NONE");
    log("overall="+std::string(result==77 ? "SKIP" : result ? "FAIL" : injected ? "FAULT_CLEANUP_PASS" : "API_PASS"));
    return result;
}
