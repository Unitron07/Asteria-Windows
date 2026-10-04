#include "pyrowave_runtime.h"
#include "pyrowave_gpu.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdlib>
#include <iostream>

namespace PyroWave {
struct RuntimeTestAccess {
    // Supply an explicit test module instead of a system Vulkan loader. Production
    // loading still requires System32; no environment switch or loader bypass.
    static bool attachMockVulkan(Runtime& r,const std::filesystem::path& dll) {
        r.m_Vulkan=LoadLibraryExW(dll.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        return r.m_Vulkan!=nullptr;
    }
    static void disableStats(Runtime& r) { r.m_Api.reportStats=nullptr; }
    static bool gpuDecode(Runtime& r,const std::vector<uint8_t>& bytes) {
        Pixels unused; pyrowave_gpu_buffers buffers{};
        return r.decodeImpl(bytes,unused,true,nullptr,nullptr,&buffers);
    }
};
}
static void require(bool ok) { if (!ok) { std::cerr<<"FAIL runtime policy\n"; std::exit(1); } }
static void word(std::vector<uint8_t>& bytes,uint32_t n) { for (int i=0;i<4;++i) bytes.push_back(uint8_t(n>>(8*i))); }
static void collect(void* userdata,const char* message) { static_cast<std::vector<std::string>*>(userdata)->emplace_back(message); }
int main(int argc,char** argv) {
    require(argc==2);
    const auto directory=std::filesystem::absolute(argv[1]);
    const auto dll=directory / L"libpyrowave-shared-0.dll";
    HMODULE module=LoadLibraryExW(dll.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    require(module!=nullptr);
    auto configure=reinterpret_cast<void(*)(bool,uint32_t,bool)>(GetProcAddress(module,"mock_configure"));
    require(configure!=nullptr);
    // Identical vendor with either recommendation, and different vendors with the
    // same recommendation: no vendor heuristic may determine decoder selection.
    for (const uint32_t vendor:{0x5143u,0x10deu,0u}) for (bool fragment:{false,true}) {
        configure(fragment,vendor,false);
        PyroWave::Runtime runtime;
        require(runtime.load(directory));
        require(PyroWave::RuntimeTestAccess::attachMockVulkan(runtime,dll));
        require(runtime.createDecoder(128,128));
        const std::string path=fragment ? "fragment" : "compute";
        require(runtime.decoderPath()==path);
        {
            PyroWave::GpuPresentation gpu(runtime);
            // Mock intentionally lacks image-import exports. Initialization fails
            // before accessing SDL; the same decoder must remain usable afterward.
            require(!gpu.initialize(nullptr,128,128));
            require(gpu.error().find("missing GPU API")!=std::string::npos);
        }
        require(runtime.decoderPath()==path);
        for (bool limited:{false,true}) {
            require(runtime.createDecoder(128,128)); // Also cover restoration/reconnect.
            require(runtime.decoderPath()==path);
            std::vector<uint8_t> bytes;
            word(bytes,1); word(bytes,8); word(bytes,0x80000000u|(127u<<14)|127u); word(bytes,limited ? 1u<<30 : 0u);
            PyroWave::Pixels pixels; std::size_t packets=0;
            for (int repeat=0;repeat<3;++repeat) {
                require(runtime.decodeLive(bytes,pixels,packets));
                require(packets==1 && pixels.width==128 && pixels.height==128);
                require(pixels.range==(limited ? PyroWave::YuvRange::Limited : PyroWave::YuvRange::Full));
                for (int p=0;p<3;++p) {
                    require(pixels.planes[p].size()==std::size_t(p ? 64*64 : 128*128));
                    for (auto byte:pixels.planes[p]) require(byte==(p ? 128 : 16));
                }
            }
            // Callback plumbing is identical for CPU and GPU output.
            require(PyroWave::RuntimeTestAccess::gpuDecode(runtime,bytes));
            std::vector<std::string> messages;
            require(runtime.reportPerformanceStats(collect,&messages));
            require(messages==std::vector<std::string>{"Dequant: 0.125 ms per frame",
                fragment ? "iDWT fragment: 0.250 ms per frame" : "iDWT: 0.250 ms per frame","reset=false"});
            messages.clear(); require(runtime.reportPerformanceStats(collect,&messages,true));
            require(messages.back()=="reset=true");
            configure(fragment,vendor,true);
            messages.clear(); require(runtime.reportPerformanceStats(collect,&messages) && messages.empty());
            require(runtime.error().empty() && runtime.decodeLive(bytes,pixels,packets));
            configure(fragment,vendor,false);
        }
        PyroWave::RuntimeTestAccess::disableStats(runtime);
        require(!runtime.reportPerformanceStats(collect,nullptr) && runtime.error().empty());
        runtime.resetDecoder();
        require(runtime.createDecoder(128,128) && runtime.decoderPath()==path);
        runtime.close();
        require(!runtime.reportPerformanceStats(collect,nullptr));
    }
    FreeLibrary(module);
    std::cout<<"PASS: API-driven fragment/compute, nonfatal GPU fallback, I420/ranges, optional native callbacks\n";
}
