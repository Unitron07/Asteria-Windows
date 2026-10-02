#include "pyrowave_queue.h"
#include "pyrowave_stats.h"
#include <cstdint>
#include <cstdlib>
#include <iostream>
struct Stats {
    uint32_t receivedFrames=0,decodedFrames=0,renderedFrames=0,totalFrames=0,networkDroppedFrames=0,pacerDroppedFrames=0;
    uint16_t minHostProcessingLatency=0,maxHostProcessingLatency=0;
    uint32_t totalHostProcessingLatency=0,framesWithHostProcessingLatency=0,lastRtt=0,lastRttVariance=0;
    uint64_t totalReassemblyTimeUs=0,totalDecodeTimeUs=0,totalPacerTimeUs=0,totalRenderTimeUs=0,measurementStartUs=0;
};
static void require(bool ok) { if (!ok) { std::cerr<<"FAIL queue/stats\n"; std::exit(1); } }
int main() {
    PyroWave::FrameSlots slots; bool dropped;
    int a=slots.reserve(dropped); require(a>=0 && !dropped);
    require(!slots.publish(a)); require(slots.take()==a); slots.displayed(a);
    int b=slots.reserve(dropped); require(b!=a && !dropped); slots.publish(b);
    int c=slots.reserve(dropped); require(c!=a && c!=b && !dropped);
    require(slots.publish(c)); require(slots.state(b)==PyroWave::FrameSlots::State::Free);
    require(slots.take()==c);
    int d=slots.reserve(dropped); require(d==b && !dropped); slots.publish(d);
    // Displayed + rendering + pending occupies all three. Recycle only pending.
    int e=slots.reserve(dropped); require(e==d && dropped);
    slots.cancel(e); slots.displayed(c);
    require(slots.current()==c && slots.state(a)==PyroWave::FrameSlots::State::Free);
    for (int n=0;n<10000;++n) {
        const int i=slots.reserve(dropped); require(i>=0 && i!=slots.current());
        slots.publish(i);
        if (n%3==0) { require(slots.take()==i); slots.displayed(i); }
    }
    Stats active,last,merged;
    PyroWave::hostLatency(active,0); require(!active.framesWithHostProcessingLatency);
    PyroWave::hostLatency(active,100); PyroWave::hostLatency(active,250);
    PyroWave::hostLatency(last,50);
    last.measurementStartUs=1000000; active.measurementStartUs=2000000;
    active.receivedFrames=90; active.totalFrames=100; active.networkDroppedFrames=10;
    active.decodedFrames=80; active.renderedFrames=60; active.pacerDroppedFrames=20;
    active.totalDecodeTimeUs=160000; active.totalPacerTimeUs=180000; active.totalRenderTimeUs=240000;
    PyroWave::addStats(last,merged); PyroWave::addStats(active,merged);
    require(merged.minHostProcessingLatency==50 && merged.maxHostProcessingLatency==250);
    require(merged.totalHostProcessingLatency==400 && merged.framesWithHostProcessingLatency==3);
    require(PyroWave::percent(10,100)==10 && PyroWave::percent(20,80)==25 && PyroWave::percent(0,0)==0);
    require(PyroWave::elapsed(2000,1000)==1000 && PyroWave::elapsed(1000,2000)==0);
    const auto text=PyroWave::formatStats(merged,{},2560,1440,3000000,true);
    for (const auto expected:{"2560x1440 50.00 FPS","network: 45.00 FPS","5.0/25.0/13.3 ms","connection: 10.00%",
            "jitter: 25.00%","Average decoding time: 2.00 ms","Average frame queue delay: 3.00 ms",
            "V-sync latency): 4.00 ms","network latency: N/A","GPU execution not measured"})
        require(text.find(expected)!=std::string::npos);
    merged.lastRtt=13; merged.lastRttVariance=5;
    const auto cpu=PyroWave::formatStats(merged,{},2560,1440,3000000,false);
    require(cpu.find("13 ms (variance: 5 ms)")!=std::string::npos && cpu.find("readback not separable")!=std::string::npos);
    require(PyroWave::formatStats(Stats{}, {},128,128,1000000,false).find("Average decoding time: N/A")!=std::string::npos);
    std::cout<<"PASS: bounded latest-frame queue, retained redraw, stats windows, host latency, drops and timing\n";
}
