#include "pyrowave_runtime.h"
#include "shared_policy.h"
#include <windows.h>
#include <iostream>
namespace PyroWave {
struct RuntimeTestAccess {
    static void mockLoader(Runtime& runtime,const std::filesystem::path& dll) {
        runtime.m_Vulkan=LoadLibraryExW(dll.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!runtime.m_Vulkan) throw std::runtime_error("mock loader unavailable");
    }
    static void missingShared(Runtime& runtime) { runtime.m_Api.decodeGpu=nullptr; }
};
}
static void require(bool ok) { if(!ok) throw std::runtime_error("shared-runtime mock failure"); }
int main(int argc,char** argv) {
    try {
        require(argc==3); auto dir=std::filesystem::absolute(argv[1]); auto dll=dir/L"libpyrowave-shared-0.dll";
        HMODULE module=LoadLibraryExW(dll.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32); require(module!=nullptr);
        auto configure=reinterpret_cast<void(*)(int,bool)>(GetProcAddress(module,"mock_configure"));
        auto counts=reinterpret_cast<void(*)(int*,int*,int*,int*)>(GetProcAddress(module,"mock_counts")); require(configure && counts);
        Stage3::QueueLock lock;
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO}; VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; ici.pApplicationInfo=&app;
        VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        pyrowave_device_create_queue_info queue{}; queue.queue=reinterpret_cast<VkQueue>(uintptr_t(4));
        pyrowave_device_create_info info{}; info.instance=reinterpret_cast<VkInstance>(uintptr_t(1));
        info.physical_device=reinterpret_cast<VkPhysicalDevice>(uintptr_t(2)); info.device=reinterpret_cast<VkDevice>(uintptr_t(3));
        info.GetInstanceProcAddr=reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(module,"vkGetInstanceProcAddr"));
        info.instance_create_info=&ici; info.device_create_info=&dci; info.queue_info=&queue; info.queue_info_count=1;
        info.userdata=&lock; info.queue_lock_callback=[](void* p) { static_cast<Stage3::QueueLock*>(p)->lock(true); };
        info.queue_unlock_callback=[](void* p) { static_cast<Stage3::QueueLock*>(p)->unlock(true); };
        for(bool fragment:{false,true}) for(int mode=0;mode<8;++mode) {
            configure(mode,fragment); PyroWave::Runtime runtime; require(runtime.load(dir));
            PyroWave::RuntimeTestAccess::mockLoader(runtime,dll);
            if(mode==7) PyroWave::RuntimeTestAccess::missingShared(runtime);
            const bool borrowed=runtime.borrowDevice(info);
            require(borrowed==(mode==0 || mode==5 || mode==6));
            if (mode>=2 && mode<=4) require(runtime.error().find("handle mismatch")!=std::string::npos);
            if(mode==7) require(runtime.error().find("missing shared-device export")!=std::string::npos);
            if(borrowed) {
                { Stage3::QueueLock::Guard caller(lock); }
                require(runtime.createDecoder(128,128,true));
                require(std::string(runtime.decoderPath())==(fragment ? "fragment" : "compute"));
                require(runtime.nativePrefersFragment()==fragment);
                require(runtime.createDecoder(128,128,false));
                require(std::string(runtime.decoderPath())=="compute" && runtime.nativePrefersFragment()==fragment);
                require(runtime.createDecoder(128,128,true));
                std::vector<uint8_t> bytes;
                auto word=[&](uint32_t n) { for(int i=0;i<4;++i) bytes.push_back(uint8_t(n>>(i*8))); };
                word(1); word(8); word(0x80000000u|(127u<<14)|127u); word(0);
                pyrowave_gpu_buffers buffers{}; pyrowave_gpu_sync_operation a{},r{};
                r.sync={reinterpret_cast<VkSemaphore>(uintptr_t(5)),2}; a.sync={r.sync.semaphore,1};
                require(runtime.decodeNative(bytes,buffers,a,r)==(mode==0));
                auto bad=bytes; bad.pop_back(); require(!runtime.decodeNative(bad,buffers,a,r) && runtime.frameRejected());
                configure(0,fragment); require(runtime.decodeNative(bytes,buffers,a,r));
                size_t packets=0; PyroWave::DecodeTiming timing;
                require(runtime.decodeLiveNative(bytes,buffers,a,r,packets,timing) && packets==1);
                auto left=bytes; left.back()|=0x80;
                require(!runtime.decodeLiveNative(left,buffers,a,r,packets,timing) && runtime.frameRejected());
                auto limited=bytes; limited.back()|=0x40;
                require(!runtime.decodeLiveNative(limited,buffers,a,r,packets,timing) && runtime.frameRejected());
                require(runtime.liveRange()==PyroWave::YuvRange::Full);
                configure(5,fragment);
                require(!runtime.decodeLiveNative(bytes,buffers,a,r,packets,timing) && runtime.frameRejected());
                configure(0,fragment); require(runtime.decodeLiveNative(bytes,buffers,a,r,packets,timing));
                PyroWave::Pixels forbidden;
                require(!runtime.decodeLive(bytes,forbidden,packets,&timing) && runtime.cpuYuvReadbackFrames()==0);
                require(runtime.decodeLiveNative(bytes,buffers,a,r,packets,timing));
                r.sync.value=0; require(!runtime.decodeNative(bytes,buffers,a,r)); r.sync.value=2;
                r.num_images=1; require(!runtime.decodeNative(bytes,buffers,a,r));
                { Stage3::QueueLock::Guard caller(lock); }
            }
            runtime.close(); int defaults,gpu,cpu,destroyed; counts(&defaults,&gpu,&cpu,&destroyed);
            require(defaults==0 && cpu==0 && lock.balanced());
        }
        {
            auto missing=std::filesystem::absolute(argv[2]); PyroWave::Runtime runtime;
            require(runtime.load(missing)); PyroWave::RuntimeTestAccess::mockLoader(runtime,missing/L"libpyrowave-shared-0.dll");
            require(!runtime.borrowDevice(info) && runtime.error().find("missing shared-device export")!=std::string::npos);
        }
        FreeLibrary(module);
        std::cout<<"PASS: mock borrowing, missing exports, mismatch rejection, codec/parser errors and recovery, teardown, no default-device/CPU/external fallback\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
