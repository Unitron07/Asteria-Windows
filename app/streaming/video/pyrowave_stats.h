#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>

namespace PyroWave {
struct PipelineStats {
    uint64_t bytes=0, preparationUs=0, decoderQueueUs=0;
};
// Operates on VIDEO_STATS. Kept generic so GPU-free tests need no Qt/SDL.
template<class Stats> void addStats(const Stats& src,Stats& dst) {
#define ADD(field) dst.field+=src.field
    ADD(receivedFrames); ADD(decodedFrames); ADD(renderedFrames); ADD(totalFrames);
    ADD(networkDroppedFrames); ADD(pacerDroppedFrames); ADD(totalReassemblyTimeUs);
    ADD(totalDecodeTimeUs); ADD(totalPacerTimeUs); ADD(totalRenderTimeUs);
    ADD(totalHostProcessingLatency); ADD(framesWithHostProcessingLatency);
#undef ADD
    if (!dst.minHostProcessingLatency) dst.minHostProcessingLatency=src.minHostProcessingLatency;
    else if (src.minHostProcessingLatency) dst.minHostProcessingLatency=std::min(dst.minHostProcessingLatency,src.minHostProcessingLatency);
    dst.maxHostProcessingLatency=std::max(dst.maxHostProcessingLatency,src.maxHostProcessingLatency);
    if (!dst.measurementStartUs) dst.measurementStartUs=src.measurementStartUs;
}
template<class Stats> void hostLatency(Stats& stats,uint16_t latency) {
    if (!latency) return;
    stats.minHostProcessingLatency=stats.minHostProcessingLatency ? std::min(stats.minHostProcessingLatency,latency) : latency;
    stats.maxHostProcessingLatency=std::max(stats.maxHostProcessingLatency,latency);
    stats.totalHostProcessingLatency+=latency;
    ++stats.framesWithHostProcessingLatency;
}
inline double percent(uint32_t numerator,uint32_t denominator) {
    return denominator ? numerator*100.0/denominator : 0;
}
inline uint64_t elapsed(uint64_t end,uint64_t start) { return end>=start ? end-start : 0; }
template<class Stats> std::string formatStats(const Stats& stats,const PipelineStats& pipeline,
    int width,int height,uint64_t now,bool gpu) {
    std::string output;
    auto append=[&](const char* format,auto... args) {
        char line[512]; std::snprintf(line,sizeof(line),format,args...); output+=line;
    };
    const double seconds=elapsed(now,stats.measurementStartUs)/1000000.0;
    if (seconds<=0) return output;
    append("Video stream: %dx%d %.2f FPS (Codec: PyroWave SDR 8-bit 4:2:0)\n",width,height,stats.totalFrames/seconds);
    append("Incoming frame rate from network: %.2f FPS\nDecoding frame rate: %.2f FPS\nRendering frame rate: %.2f FPS\n",
        stats.receivedFrames/seconds,stats.decodedFrames/seconds,stats.renderedFrames/seconds);
    if (stats.framesWithHostProcessingLatency)
        append("Host processing latency min/max/average: %.1f/%.1f/%.1f ms\n",stats.minHostProcessingLatency/10.0,
            stats.maxHostProcessingLatency/10.0,stats.totalHostProcessingLatency/10.0/stats.framesWithHostProcessingLatency);
    if (stats.totalFrames) append("Frames dropped by your network connection: %.2f%%\n",percent(stats.networkDroppedFrames,stats.totalFrames));
    else append("Frames dropped by your network connection: N/A\n");
    if (stats.decodedFrames) append("Frames dropped due to network jitter: %.2f%%\n",percent(stats.pacerDroppedFrames,stats.decodedFrames));
    else append("Frames dropped due to network jitter: N/A\n");
    if (stats.lastRtt) append("Average network latency: %u ms (variance: %u ms)\n",stats.lastRtt,stats.lastRttVariance);
    else append("Average network latency: N/A\n");
    if (stats.decodedFrames) append("Average decoding time: %.2f ms\n",stats.totalDecodeTimeUs/1000.0/stats.decodedFrames);
    else append("Average decoding time: N/A\n");
    if (stats.renderedFrames) {
        append("Average frame queue delay: %.2f ms\nAverage rendering time (including monitor V-sync latency): %.2f ms\n",
            stats.totalPacerTimeUs/1000.0/stats.renderedFrames,stats.totalRenderTimeUs/1000.0/stats.renderedFrames);
    } else append("Average frame queue delay: N/A\nAverage rendering time (including monitor V-sync latency): N/A\n");
    append("Bitrate: %.1f Mbps\n",pipeline.bytes*8.0/seconds/1000000.0);
    if (stats.receivedFrames) append("Average network reassembly / decoder queue wait: %.2f / %.2f ms\n",
        stats.totalReassemblyTimeUs/1000.0/stats.receivedFrames,pipeline.decoderQueueUs/1000.0/stats.receivedFrames);
    if (stats.decodedFrames) append("Average parser / frame preparation: %.2f ms\n",pipeline.preparationUs/1000.0/stats.decodedFrames);
    append("Decode timing: %s\n",gpu ? "CPU GPU-decode submission; GPU execution not measured" : "CPU API decode + readback; readback not separable");
    return output;
}
}
