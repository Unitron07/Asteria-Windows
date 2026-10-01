#include "presentation_patterns.h"
#include <iostream>
#include <stdexcept>
#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while (0)
int main() {
    try {
        using namespace Presentation;
        for (int k=0;k<5;++k) {
            const auto p=pattern(Pattern(k)), q=pattern(Pattern(k));
            CHECK(PyroWave::validProofPixels(p));
            for (int plane=0;plane<3;++plane) CHECK(p.planes[plane]==q.planes[plane]);
            auto bad=p; bad.planes[1].pop_back(); CHECK(!PyroWave::validProofPixels(bad));
            bad=p; bad.width=1921; CHECK(!PyroWave::validProofPixels(bad));
        }
        const auto range=pattern(Pattern::Range);
        CHECK(range.planes[0][0]==0 && range.planes[0][1919]==255);
        CHECK(range.planes[0][412]==16 && range.planes[0][1380]==235);
        const auto bars=pattern(Pattern::Bars);
        CHECK(bars.planes[0][1200]==63 && bars.planes[1][600]==102 && bars.planes[2][600]==240);
        CHECK(bars.planes[1][720]==240 && bars.planes[2][720]==118);
        const auto chroma=pattern(Pattern::Chroma);
        CHECK(chroma.planes[1][59]==192 && chroma.planes[1][60]==64);
        CHECK(chroma.planes[1][270*960]!=chroma.planes[1][270*960+1]);
        const auto grid=pattern(Pattern::Geometry);
        CHECK(grid.planes[0][10*1920+10]!=grid.planes[0][10*1920+11]);
        CHECK(grid.planes[0][600*1920+10]==128); // grid row
        auto r=fit(1000,1000); CHECK(r.w==1000 && r.h==562 && r.y==219);
        r=fit(2560,1440); CHECK(r.w==2560 && r.h==1440 && r.x==0 && r.y==0);
        r=fit(800,400); CHECK(r.w==711 && r.h==400 && r.x==44);
        CHECK(fit(0,0).w==0);
        CHECK(!recreateRenderer(Reset::Targets) && !recreateRenderer(Reset::Device));
        CHECK(recreateRenderer(Reset::Resources));
        Timing t; t.add(16); t.add(17); t.add(50);
        CHECK(t.intervals==3 && t.minimum==16 && t.maximum==50 && t.missed==2);
        std::cout<<"PASS deterministic I420 patterns, extents/pitches, fit rectangles, reset policy and timing\n";
        return 0;
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
