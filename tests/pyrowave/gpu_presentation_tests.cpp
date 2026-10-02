#define SDL_MAIN_HANDLED
#include "pyrowave_gpu.h"
#include "pyrowave_sdl.h"
#include <iostream>
#include <filesystem>
#include <cstdlib>
static void check(bool ok,int line) { if (!ok) { std::cerr<<"FAIL GPU presentation at line "<<line<<": "<<SDL_GetError()<<'\n'; std::exit(1); } }
#define require(ok) check((ok),__LINE__)
int main(int argc,char** argv) {
    SDL_SetMainReady(); require(SDL_Init(SDL_INIT_VIDEO)==0);
    auto window=SDL_CreateWindow("PyroWave shared GPU proof",0,0,1920,1080,SDL_WINDOW_HIDDEN);
    require(window!=nullptr);
    PyroWave::Runtime runtime;
    auto software=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE); require(software!=nullptr);
    {
        PyroWave::GpuPresentation gpu(runtime);
        require(!gpu.initialize(software,1920,1080));
        // Failed GPU preflight leaves ordinary full/limited CPU texture creation valid.
        for (auto range:{PyroWave::YuvRange::Full,PyroWave::YuvRange::Limited}) {
            auto texture=PyroWave::createLiveTexture(software,1920,1080,range); require(texture!=nullptr);
            SDL_DestroyTexture(texture);
        }
    }
    SDL_DestroyRenderer(software);
    if (argc==1) { SDL_DestroyWindow(window); SDL_Quit(); std::cout<<"PASS: unsupported GPU initialization preserves CPU fallback\n"; return 0; }
    require(argc==2);
    if (!runtime.load(std::filesystem::absolute(argv[1]))) return 1;
    if (!runtime.createDecoder(1920,1080)) { std::cerr<<"SKIP: Vulkan unavailable: "<<runtime.error()<<'\n'; return 77; }
    int driver=-1;
    for (int i=0;i<SDL_GetNumRenderDrivers();++i) { SDL_RendererInfo info{}; SDL_GetRenderDriverInfo(i,&info); if (std::string(info.name)=="direct3d11") driver=i; }
    if (driver<0) return 77;
    auto renderer=SDL_CreateRenderer(window,driver,SDL_RENDERER_ACCELERATED);
    if (!renderer) return 77;
    std::vector<uint8_t> fixture;
    require(runtime.generateProofFrame(fixture));
    std::cerr<<"GPU proof: codec fixture generated\n";
    PyroWave::Frame frame; std::string error;
    require(PyroWave::parseFrame(fixture.data(),fixture.size(),fixture.size(),frame,error));
    require(!frame.records.empty());
    const auto rangeByte=frame.records[0].offset+7;
    for (auto range:{PyroWave::YuvRange::Full,PyroWave::YuvRange::Limited}) {
        require(runtime.createDecoder(1920,1080));
        PyroWave::GpuPresentation gpu(runtime);
        if (!gpu.initialize(renderer,1920,1080)) {
            std::cerr<<"SKIP: shared GPU path unsupported: "<<gpu.error()<<'\n'; runtime.resetDecoder(); return 77;
        }
        std::cerr<<"GPU proof: shared planes/fences initialized\n";
        if (range==PyroWave::YuvRange::Full) fixture[rangeByte]&=~0x40u; else fixture[rangeByte]|=0x40u;
        for (int n=0;n<24;++n) {
            const int slot=n%3; std::size_t packets=0; PyroWave::DecodeTiming timing;
            require(gpu.decode(slot,fixture,packets,timing)); require(packets>0 && runtime.liveRange()==range);
            require(gpu.beginRender(slot));
            require(SDL_RenderCopy(renderer,gpu.texture(slot),nullptr,nullptr)==0);
            // Diagnostic readback exists only in this proof, never in live GPU presentation.
            if (n==23) {
                std::vector<uint32_t> rgb(1920*1080);
                require(SDL_RenderReadPixels(renderer,nullptr,SDL_PIXELFORMAT_ARGB8888,rgb.data(),1920*4)==0);
                const int black=rgb[540*1920+8]&255,white=rgb[540*1920+1912]&255;
                std::cerr<<"GPU proof: rendered endpoints "<<black<<" / "<<white<<" range="<<int(range)<<'\n';
                require(std::abs(black-(range==PyroWave::YuvRange::Full ? 16 : 0))<=5);
                require(std::abs(white-(range==PyroWave::YuvRange::Full ? 235 : 255))<=5);
            }
            SDL_RenderPresent(renderer); require(gpu.endRender(slot));
        }
        runtime.resetDecoder();
        std::cout<<"PASS: GPU R8 import/decode/fence/reuse and rendered "<<(range==PyroWave::YuvRange::Full ? "full" : "limited")<<" endpoints\n";
    }
    SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
}
