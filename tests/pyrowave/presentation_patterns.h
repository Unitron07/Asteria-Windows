#pragma once
#include "pyrowave_pixels.h"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace Presentation {
enum class Pattern { Range, Bars, Chroma, Geometry, Gradient };
inline const char* name(Pattern p) {
    static const char* names[]={"range","bt709-bars","centered-chroma","geometry","gradient"};
    return names[static_cast<int>(p)];
}
inline PyroWave::Pixels pattern(Pattern kind) {
    PyroWave::Pixels p;
    p.width=1920; p.height=1080;
    p.planes[0].resize(2073600,128);
    p.planes[1].resize(518400,128); p.planes[2].resize(518400,128);
    // Rounded full-amplitude RGB bars in 8-bit BT.709 limited YCbCr.
    constexpr int bars[8][3]={{235,128,128},{219,16,138},{188,154,16},{173,42,26},
                             {78,214,230},{63,102,240},{32,240,118},{16,128,128}};
    constexpr int levels[]={0,8,15,16,17,32,64,128,192,234,235,236,247,255};
    for (int y=0;y<1080;++y) for (int x=0;x<1920;++x) {
        int value=128;
        switch (kind) {
        case Pattern::Range: value=levels[(x/2)*14/960]; break;
        case Pattern::Bars: value=bars[x/240][0]; break;
        case Pattern::Chroma:
            // Luma fiducials coincide with even 2x2 chroma boundaries.
            value=(x%120<2 || y%120<2) ? 235 : 128;
            break;
        case Pattern::Geometry: {
            // Localized fine checks keep the offline fixture inside the unchanged
            // 850 KB / 1024-packet cap even with difficult wavelet coefficients.
            const int step=y<540 ? 1 : 2;
            value=(x<480 && (y<240 || y>=840)) ? (((x/step+y/step)&1) ? 235 : 16) : 16;
            if (x%120==0 || y%120==0 || x==960 || y==540 ||
                x<2 || y<2 || x>=1918 || y>=1078 ||
                ((x==760 || x==1160) && y>=340 && y<=740) ||
                ((y==340 || y==740) && x>=760 && x<=1160)) value=128;
            break;
        }
        case Pattern::Gradient: value=16+219*x/1919; break;
        default: throw std::invalid_argument("invalid pattern");
        }
        p.planes[0][y*1920+x]=std::uint8_t(value);
    }
    for (int y=0;y<540;++y) for (int x=0;x<960;++x) {
        if (kind==Pattern::Bars) {
            p.planes[1][y*960+x]=std::uint8_t(bars[x/120][1]);
            p.planes[2][y*960+x]=std::uint8_t(bars[x/120][2]);
        } else if (kind==Pattern::Chroma) {
            // One sample is centered at (2*x+0.5,2*y+0.5) in luma-index coordinates.
            // Broad transitions above; 2x2 and 4x4 alternating quads below.
            const bool alternate=y<270 ? ((x/60+y/60)&1) : ((x/(y<405 ? 1 : 2)+y/(y<405 ? 1 : 2))&1);
            p.planes[1][y*960+x]=alternate ? 64 : 192;
            p.planes[2][y*960+x]=alternate ? 192 : 64;
        }
    }
    return p;
}

struct Rect { int x=0,y=0,w=0,h=0; };
inline Rect fit(int w,int h) {
    if (w<=0 || h<=0) return {};
    Rect r;
    if (std::int64_t(w)*1080<=std::int64_t(h)*1920) { r.w=w; r.h=int(std::int64_t(w)*1080/1920); }
    else { r.h=h; r.w=int(std::int64_t(h)*1920/1080); }
    r.x=(w-r.w)/2; r.y=(h-r.h)/2; return r;
}
// API-independent policy is covered headlessly; actual reset events are handled by SDL.
enum class Reset { Targets, Device, Resources };
inline bool recreateRenderer(Reset reset) { return reset==Reset::Resources; }
struct Timing {
    std::uint64_t intervals=0, missed=0;
    double sum=0, minimum=0, maximum=0;
    void add(double ms) {
        if (ms<0) throw std::invalid_argument("negative interval");
        if (!intervals || ms<minimum) minimum=ms;
        maximum=std::max(maximum,ms); sum+=ms; ++intervals;
        // Number of nominal 60 Hz intervals lost, with 10% jitter tolerance.
        const auto nominal=std::uint64_t(ms/(1000.0/60.0)+0.1);
        if (nominal>1) missed+=nominal-1;
    }
};
}
