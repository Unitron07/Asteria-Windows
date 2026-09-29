#include "pyrowave_runtime.h"
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    if (argc<3) {
        std::cerr << "Usage: pyrowave-offline-proof --expect-missing|--load|--roundtrip ABSOLUTE_DLL_DIRECTORY [OUTPUT_DIRECTORY]\n";
        return 2;
    }
    const std::string mode=argv[1];
    if (mode!="--expect-missing" && mode!="--load" && mode!="--roundtrip") return 2;
    PyroWave::Runtime runtime;
    const auto directory=std::filesystem::absolute(argv[2]);
    if (mode=="--expect-missing") {
        if (runtime.load(directory)) return 1;
        std::cout << "PASS: missing DLL recovered; process remains running\n"; return 0;
    }
    if (!runtime.load(directory)) return 1;
    std::cout << "PASS: restricted runtime load, required exports, API 0.6.0\n";
    if (mode=="--load") {
        runtime.close();
        if (!runtime.load(directory)) return 1;
        std::cout << "PASS: unload/reload\n"; return 0;
    }
    if (!runtime.createDecoder(1920,1080)) return 1;
    std::vector<std::uint8_t> frame;
    if (!runtime.generateProofFrame(frame)) return 1;
    for (int cycle=0;cycle<3;++cycle) {
        if (cycle && !runtime.createDecoder(1920,1080)) return 1;
        PyroWave::Pixels pixels;
        if (!runtime.decode(frame,pixels)) return 1;
        if (pixels.width!=1920 || pixels.height!=1080 || pixels.planes[0].size()!=2073600 ||
            pixels.planes[1].size()!=518400 || pixels.planes[2].size()!=518400) return 1;
        // Validate actual decoded content against generated pattern with lossy tolerance.
        std::uint64_t error=0;
        for (int p=0;p<3;++p) {
            const std::size_t w=p ? 960 : 1920;
            for (std::size_t i=0;i<pixels.planes[p].size();++i) {
                const int expected=p ? 128 : int(16+219*(i%w)/(w-1));
                const int delta=int(pixels.planes[p][i])-expected;
                error+=std::uint64_t(delta<0 ? -delta : delta);
            }
        }
        const double mae=double(error)/3110400.0;
        if (mae>8.0) { std::cerr << "FAIL: roundtrip mean absolute error " << mae << '\n'; return 1; }
        std::cout << "PASS: cycle=" << cycle << " frameBytes=" << frame.size()
                  << " extent=1920x1080 I420 planeBytes=2073600,518400,518400 MAE=" << mae << '\n';
        if (!cycle && argc==4) {
            const auto output=std::filesystem::absolute(argv[3]);
            std::filesystem::create_directories(output);
            std::ofstream fixture(output/"generated-1080p.pyrw",std::ios::binary);
            fixture.write(reinterpret_cast<const char*>(frame.data()),std::streamsize(frame.size()));
            std::ofstream yuv(output/"decoded-1080p.i420",std::ios::binary);
            for (const auto& p : pixels.planes)
                yuv.write(reinterpret_cast<const char*>(p.data()),std::streamsize(p.size()));
            if (!fixture || !yuv) return 1;
        }
        // Reject malformed input, then prove the next complete frame still decodes.
        auto bad=frame; bad[7]=1;
        if (runtime.decode(bad,pixels) || !pixels.planes[0].empty()) return 1;
        if (!runtime.decode(frame,pixels)) return 1;
        runtime.resetDecoder();
    }
    runtime.close();
    std::cout << "PASS: known CPU pixel buffer copied; SDL IYUV-compatible (no SDL window/pacing test)\n";
    return 0;
}
