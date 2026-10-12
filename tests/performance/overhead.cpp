#include "pyrowave_perf.h"
#include <iostream>
#include <memory>
// A repeatable CPU-only surrogate, not a codec/GPU or live-stream qualification.
static volatile uint64_t sink=0;
static double run(PyroWavePerf::Capture* capture) {
    constexpr unsigned Frames=200000;
    auto start=std::chrono::steady_clock::now();
    for(unsigned frame=0;frame<Frames;++frame) {
        uint64_t work=frame;
        for(unsigned i=0;i<64;++i) work=work*1664525+1013904223;
        sink=work;
        if(capture) {
            capture->record(PyroWavePerf::Assembly,frame%50);
            capture->record(PyroWavePerf::Preparation,frame%100);
            capture->record(PyroWavePerf::GpuDecodeSubmission,frame%500);
            capture->interval(PyroWavePerf::PublicationInterval,PyroWavePerf::nowUs(),capture->lastPublication);
            capture->increment(PyroWavePerf::Decoded);
            capture->event(PyroWavePerf::Publish,frame,frame%3);
            capture->submitted(true,false,frame,frame%3);
        }
    }
    return std::chrono::duration<double,std::nano>(std::chrono::steady_clock::now()-start).count()/Frames;
}
int main() {
    auto capture=std::make_unique<PyroWavePerf::Capture>();
    capture->start=PyroWavePerf::nowUs(); capture->warmup=0; capture->duration=3600000000;
    run(nullptr); run(capture.get());
    std::cout << "{\"schemaVersion\":1,\"workload\":\"CPU-only synthetic; 64 arithmetic operations and capture hooks per frame\",\"unit\":\"ns/frame\",\"pairs\":[";
    for(int i=0;i<7;++i) {
        double disabled,enabled;
        if(i%2) { enabled=run(capture.get()); disabled=run(nullptr); }
        else { disabled=run(nullptr); enabled=run(capture.get()); }
        if(i) std::cout << ',';
        std::cout << "{\"disabled\":" << disabled << ",\"enabled\":" << enabled << ",\"added\":" << enabled-disabled << '}';
    }
    std::cout << "]}\n";
}
