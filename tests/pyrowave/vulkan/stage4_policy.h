#pragma once
#include "pyrowave_vulkan_video_policy.h"
#include "../presentation_patterns.h"
namespace Stage4 {
using Rgb = std::array<uint8_t,3>;
// Independent double reference derives RGB from Kr/Kb, not shader expressions.
inline Rgb reference(double y8, double u8, double v8, PyroWave::YuvRange range) {
    const double kr = 0.2126, kb = 0.0722, kg = 1.0-kr-kb;
    const bool limited = range == PyroWave::YuvRange::Limited;
    const double y = limited ? (y8-16.0)/219.0 : y8/255.0;
    const double cb = limited ? (u8-128.0)/224.0 : u8/255.0-0.5;
    const double cr = limited ? (v8-128.0)/224.0 : v8/255.0-0.5;
    const double r = y + 2.0*(1.0-kr)*cr;
    const double b = y + 2.0*(1.0-kb)*cb;
    const double g = (y-kr*r-kb*b)/kg;
    const auto code = [](double v) { return uint8_t(std::floor(std::clamp(v,0.0,1.0)*255.0+0.5)); };
    return {code(r),code(g),code(b)};
}
inline double sample(const std::vector<uint8_t>& plane, unsigned w, unsigned h,
                     double u, double v, Filter filter) {
    const auto at = [&](int x,int y) { return double(plane.at(size_t(std::clamp(y,0,int(h)-1))*w+std::clamp(x,0,int(w)-1))); };
    if (filter == Filter::Nearest) return at(int(std::floor(u*w)),int(std::floor(v*h)));
    const double x=u*w-0.5,y=v*h-0.5;
    const int ix=int(std::floor(x)),iy=int(std::floor(y));
    const double fx=x-ix,fy=y-iy;
    return (at(ix,iy)*(1-fx)+at(ix+1,iy)*fx)*(1-fy)+(at(ix,iy+1)*(1-fx)+at(ix+1,iy+1)*fx)*fy;
}
inline Rgb referencePixel(const PyroWave::Pixels& p,double u,double v,Filter filter,bool left=false) {
    return reference(sample(p.planes[0],1920,1080,u,v,filter),
        sample(p.planes[1],960,540,u+(left ? 0.5/1920 : 0),v,filter),
        sample(p.planes[2],960,540,u+(left ? 0.5/1920 : 0),v,filter),p.range);
}
inline PyroWave::Pixels fixture(Presentation::Pattern pattern,PyroWave::YuvRange range) {
    auto p=Presentation::pattern(pattern); p.range=range;
    // Existing suite is authored in limited codes; preserve raw range wedges.
    if (range==PyroWave::YuvRange::Full && pattern!=Presentation::Pattern::Range) {
        for(unsigned plane=0;plane<3;++plane) for(auto& value:p.planes[plane]) {
            const double code=plane ? (double(value)-128)*255/224+127.5 : (double(value)-16)*255/219;
            value=uint8_t(std::floor(std::clamp(code,0.0,255.0)+0.5));
        }
    }
    return p;
}
}
