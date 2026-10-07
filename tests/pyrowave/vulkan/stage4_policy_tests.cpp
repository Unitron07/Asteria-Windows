#include "stage4_policy.h"
#include "../../../app/streaming/video/pyrowave_vulkan_policy.h"
#include <iostream>
using namespace Stage4;
#define REQUIRE(x) do { if(!(x)) throw std::runtime_error(#x); } while(0)
int main() {
    try {
        using PyroWave::YuvRange;
        REQUIRE(reference(0,127.5,127.5,YuvRange::Full)==(Rgb{0,0,0}));
        REQUIRE(reference(255,127.5,127.5,YuvRange::Full)==(Rgb{255,255,255}));
        REQUIRE(reference(16,128,128,YuvRange::Limited)==(Rgb{0,0,0}));
        REQUIRE(reference(235,128,128,YuvRange::Limited)==(Rgb{255,255,255}));
        REQUIRE(reference(128,127.5,127.5,YuvRange::Full)==(Rgb{128,128,128}));
        REQUIRE(sdrTarget(37,0) && sdrTarget(44,0) && !sdrTarget(50,0) && !sdrTarget(43,0) && !sdrTarget(44,1000104008));
        const int bars[8][3]={{235,128,128},{219,16,138},{188,154,16},{173,42,26},{78,214,230},{63,102,240},{32,240,118},{16,128,128}};
        const Rgb expected[8]={{255,255,255},{255,255,0},{0,255,255},{0,255,0},{255,0,255},{255,0,0},{0,0,255},{0,0,0}};
        for(unsigned i=0;i<8;++i) {
            const auto rgb=reference(bars[i][0],bars[i][1],bars[i][2],YuvRange::Limited);
            for(unsigned c=0;c<3;++c) REQUIRE(withinTolerance(rgb[c],expected[i][c]));
            std::cout<<"limited-bar="<<i<<" expected="<<int(expected[i][0])<<','<<int(expected[i][1])<<','<<int(expected[i][2])<<" reference="<<int(rgb[0])<<','<<int(rgb[1])<<','<<int(rgb[2])<<'\n';
        }
        // Reviewed full-range primary/secondary code points, independent of GLSL.
        const int full[8][3]={{255,128,128},{237,0,139},{201,157,0},{182,29,12},{73,226,243},{54,98,255},{18,255,116},{0,128,128}};
        for(unsigned i=0;i<8;++i) { const auto rgb=reference(full[i][0],full[i][1],full[i][2],YuvRange::Full); for(unsigned c=0;c<3;++c) REQUIRE(withinTolerance(rgb[c],expected[i][c])); }
        REQUIRE(reference(16,128,128,YuvRange::Full)!=reference(16,128,128,YuvRange::Limited));
        REQUIRE(outputUsage(Stage3::Path::Fragment)==20 && outputUsage(Stage3::Path::Compute)==12);
        REQUIRE((outputUsage(Stage3::Path::Fragment)&3)==0);
        for(unsigned slot=0;slot<3;++slot) for(auto f:{Filter::Nearest,Filter::Linear}) REQUIRE(descriptor(slot,f)==slot*2+(f==Filter::Linear));
        bool rejected=false; try { descriptor(3,Filter::Nearest); } catch(const std::out_of_range&) { rejected=true; } REQUIRE(rejected);
        for(auto size:{std::array<int,2>{1920,1080},{2560,1080},{1000,1000},{127,93},{8192,8192}}) {
            auto r=Presentation::fit(size[0],size[1]); REQUIRE(r.x>=0 && r.y>=0 && r.w<=size[0] && r.h<=size[1]);
            REQUIRE(std::abs(int64_t(r.w)*1080-int64_t(r.h)*1920)<=1920);
        }
        for(int i=0;i<960;++i) REQUIRE(std::abs(chromaIndex(2*i+0.5)-i)<1e-12);
        REQUIRE(chromaUv(0,1920)==0.5/1920); REQUIRE(chromaUv(0,1920,true)==1.0/1920);
        const std::vector<uint8_t> plane={0,255,0,255};
        REQUIRE(sample(plane,2,2,0.5,0.5,Filter::Nearest)==255);
        REQUIRE(sample(plane,2,2,0.5,0.5,Filter::Linear)==127.5);
        REQUIRE(sample(plane,2,2,0,0,Filter::Linear)==0);
        auto chroma=fixture(Presentation::Pattern::Chroma,YuvRange::Limited);
        REQUIRE(referencePixel(chroma,120.5/1920,100.5/1080,Filter::Linear)!=referencePixel(chroma,120.5/1920,100.5/1080,Filter::Linear,true));
        std::array<Stage3::Payloads,3> slots;
        for(unsigned frame=0;frame<300;++frame) {
            const unsigned source=frame%3,image=(frame*7)%5;
            auto& s=slots[source]; const auto n=s.next(); REQUIRE(n[0]>0 && n[1]==n[0]+1 && n[2]==n[0]+2);
            s.submitted(n[1]); rejected=false; try { s.next(); } catch(...) { rejected=true; } REQUIRE(rejected);
            s.complete(n[2]); REQUIRE(image<5 && source<3);
        }
        REQUIRE(withinTolerance(128,127) && !withinTolerance(129,127));
        PyroWave::Frame frame; frame.sequence.range=YuvRange::Limited;
        frame.records.push_back({PyroWave::RecordKind::Sequence,4,8,0});
        std::vector<uint8_t> encoded(12,0); REQUIRE(metadata(encoded,frame).range==YuvRange::Limited);
        encoded[11]=0x80; rejected=false; try { metadata(encoded,frame); } catch(...) { rejected=true; } REQUIRE(rejected);
        encoded[11]=0; frame.sequence.range=YuvRange::Full; REQUIRE(metadata(encoded,frame).range==YuvRange::Full);
        PyroWaveVulkan::Progress progress; progress.rebuild=false;
        REQUIRE(progress.decide({1920,1080},true,PyroWaveVulkan::Acquire::NotReady)==PyroWaveVulkan::Action::Retry);
        REQUIRE(progress.decide({0,0},true,PyroWaveVulkan::Acquire::Ready)==PyroWaveVulkan::Action::Suspend);
        REQUIRE(progress.decide({1920,1080},true,PyroWaveVulkan::Acquire::Failed)==PyroWaveVulkan::Action::Stop);
        std::cout<<"Stage 4 CPU color/sampling/slot/state policy PASS\n"; return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
