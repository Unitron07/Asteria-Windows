#pragma once
#include "pyrowave_vulkan_shared_policy.h"
#include "pyrowave_frame.h"
#include <algorithm>
#include <cmath>

namespace Stage4 {
enum class Filter { Nearest, Linear };
constexpr unsigned RgbTolerance = 1;
inline bool sdrTarget(uint32_t format,uint32_t colorSpace) { return (format==37 || format==44) && colorSpace==0; }
// Production presentation requirements deliberately exclude TRANSFER_SRC/DST.
inline uint32_t outputUsage(Stage3::Path path) { return 4u | (path == Stage3::Path::Compute ? 8u : 16u); }
inline unsigned descriptor(unsigned slot, Filter filter) {
    if (slot >= Stage3::SlotCount) throw std::out_of_range("decode slot");
    return slot * 2 + (filter == Filter::Linear ? 1 : 0);
}
inline double chromaIndex(double lumaIndex) { return (lumaIndex - 0.5) / 2.0; }
inline double chromaUv(double lumaIndex, unsigned width, bool left = false) {
    if (!width || width % 2) throw std::invalid_argument("420 width");
    return (lumaIndex + (left ? 1.0 : 0.5)) / width;
}
inline bool withinTolerance(int observed, int reference) { return std::abs(observed-reference) <= int(RgbTolerance); }
struct Fit { int x,y,w,h; };
inline Fit fitVideo(int width,int height) {
    if(width<=0 || height<=0) return {0,0,0,0};
    int w=width,h=height;
    if(int64_t(width)*9>int64_t(height)*16) w=int(int64_t(height)*16/9);
    else h=int(int64_t(width)*9/16);
    return {(width-w)/2,(height-h)/2,w,h};
}
struct FrameMetadata { PyroWave::YuvRange range; };
// Use actual validated sequence header. Existing parser deliberately ignores siting.
inline FrameMetadata metadata(const std::vector<uint8_t>& bytes,const PyroWave::Frame& frame) {
    const auto record=std::find_if(frame.records.begin(),frame.records.end(),[](const auto& r) { return r.kind==PyroWave::RecordKind::Sequence; });
    if (record==frame.records.end() || record->offset+8>bytes.size()) throw std::runtime_error("missing sequence");
    if (bytes.at(record->offset+7)&0x80) throw std::runtime_error("Stage 4 requires CENTER chroma; LEFT rejected");
    return {frame.sequence.range};
}
}
