#include "pyrowave_runtime.h"
#include <fstream>
#include <iostream>
#ifdef PYROWAVE_SDL_PRESENTATION
#include "sdl_presentation.h"
#endif

static bool verify(const PyroWave::Pixels& pixels, double& mae) {
    if (pixels.width!=1920 || pixels.height!=1080 || pixels.planes[0].size()!=2073600 ||
        pixels.planes[1].size()!=518400 || pixels.planes[2].size()!=518400) return false;
    std::uint64_t error=0;
    for (int p=0;p<3;++p) {
        const std::size_t w=p ? 960 : 1920;
        for (std::size_t i=0;i<pixels.planes[p].size();++i) {
            const int expected=p ? 128 : int(16+219*(i%w)/(w-1));
            const int delta=int(pixels.planes[p][i])-expected;
            error+=std::uint64_t(delta<0 ? -delta : delta);
        }
    }
    mae=double(error)/3110400.0;
    return mae<=8.0;
}
int main(int argc, char** argv) {
#ifdef PYROWAVE_SDL_PRESENTATION
    if (argc>1 && std::string(argv[1]).rfind("--present",0)==0) return runPresentation(argc,argv);
#endif
    if (argc<3) {
        std::cerr << "Usage: pyrowave-offline-proof --expect-missing|--load|--roundtrip|--roundtrip-compatibility|--roundtrip-records ABSOLUTE_DLL_DIRECTORY [OUTPUT_DIRECTORY]\n";
        return 2;
    }
    const std::string mode=argv[1];
    if (mode!="--expect-missing" && mode!="--load" && mode!="--roundtrip" &&
        mode!="--roundtrip-compatibility" && mode!="--roundtrip-records") return 2;
    PyroWave::Runtime runtime;
    const auto directory=std::filesystem::absolute(argv[2]);
    if (mode=="--expect-missing") {
        if (runtime.load(directory)) return 1;
        std::cout << "PASS: missing DLL recovered; process remains running\n"; return 0;
    }
    if (!runtime.load(directory)) return 1;
    std::cout << "PASS: restricted runtime load, required exports, API 0.6.0; built bitstream ID="
              << PyroWave::BitstreamId << " codec=" << PyroWave::CodecCommit << '\n';
    if (mode=="--load") {
        runtime.close(); if (!runtime.load(directory)) return 1;
        std::cout << "PASS: unload/reload\n"; return 0;
    }
    if (!runtime.createDecoder(1920,1080)) {
        if (runtime.error().find("native system Vulkan loader unavailable")!=std::string::npos ||
            runtime.error().find("device creation failed: -5")!=std::string::npos) return 77;
        return 1;
    }
    std::cout << "Vulkan adapter: " << runtime.deviceDescription() << '\n';
    std::vector<std::uint8_t> fixtures[2];
    // Generate once; serialize the same codec packets in both host formats.
    if (!runtime.generateProofFrame(fixtures[0])) return 1;
    PyroWave::Frame frame; std::string error;
    if (!PyroWave::parseCompatibilityFrame(fixtures[0].data(),fixtures[0].size(),fixtures[0].size(),frame,error)) return 1;
    for (const auto& p:frame.packets)
        fixtures[1].insert(fixtures[1].end(),fixtures[0].begin()+p.offset,fixtures[0].begin()+p.offset+p.size);
    // Insert an empty padding record after the sequence header. It must be
    // skipped by the parser before submitting complete records to the codec.
    fixtures[1].insert(fixtures[1].begin()+8,{255,255,255,255,0,0,0,0});
    for (int cycle=0;cycle<3;++cycle) {
        if (cycle && !runtime.createDecoder(1920,1080)) return 1;
        PyroWave::Pixels reference;
        for (int format=0;format<2;++format) {
            if (mode=="--roundtrip-compatibility"&&format==1) continue;
            if (mode=="--roundtrip-records"&&format==0) continue;
            const auto& fixture=fixtures[format];
            const char* name=format ? "records" : "compatibility";
            PyroWave::Pixels pixels;
            if (!runtime.decode(fixture,pixels)) return 1;
            double mae=0;
            if (!verify(pixels,mae)) { std::cerr << "FAIL: "<<name<<" CPU output / MAE "<<mae<<'\n'; return 1; }
            if (!format) reference=pixels;
            if (format && mode=="--roundtrip")
                for (int p=0;p<3;++p) if (pixels.planes[p]!=reference.planes[p]) return 1;
            std::cout << "PASS: framing="<<name<<" cycle="<<cycle<<" frameBytes="<<fixture.size()
                      <<" extent=1920x1080 I420 planeBytes=2073600,518400,518400 MAE="<<mae<<'\n';
            if (!cycle && argc==4) {
                const auto output=std::filesystem::absolute(argv[3]); std::filesystem::create_directories(output);
                std::ofstream stream(output/(std::string("generated-1080p-")+name+".bin"),std::ios::binary);
                stream.write(reinterpret_cast<const char*>(fixture.data()),std::streamsize(fixture.size()));
                std::ofstream yuv(output/(std::string("decoded-1080p-")+name+".i420"),std::ios::binary);
                for (const auto& p:pixels.planes) yuv.write(reinterpret_cast<const char*>(p.data()),std::streamsize(p.size()));
                std::ofstream provenance(output/"codec.txt");
                provenance<<"commit="<<PyroWave::CodecCommit<<"\nbitstream="<<PyroWave::BitstreamId<<"\nAPI=0.6.0\n";
                if (!stream||!yuv||!provenance) return 1;
            }
            auto bad=fixture; bad.pop_back();
            if (runtime.decode(bad,pixels) || !pixels.planes[0].empty()) return 1;
            if (!runtime.decode(fixture,pixels)) return 1;
        }
        runtime.resetDecoder();
    }
    runtime.close();
    std::cout << "PASS: known CPU pixel buffer copied; SDL IYUV-compatible (no SDL window/pacing or network interoperability test)\n";
}
