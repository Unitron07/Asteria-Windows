#include "pyrowave_transport.h"
#include "pyrowave_pixels.h"
#include <cstdlib>
#include <iostream>
#include <algorithm>
using Bytes = std::vector<std::uint8_t>;
static void require(bool ok) { if (!ok) { std::cerr << "FAIL live frame test\n"; std::exit(1); } }
static void word(Bytes& b,std::uint32_t n) { for (int i=0;i<4;++i) b.push_back(std::uint8_t(n>>(8*i))); }
static void set(Bytes& b,std::size_t offset,std::uint32_t n) { for (int i=0;i<4;++i) b[offset+i]=std::uint8_t(n>>(8*i)); }
static Bytes sequence(unsigned blocks,unsigned w=1920,unsigned h=1080) {
    Bytes b; word(b,0x80000000u|((h-1)<<14)|(w-1)); word(b,blocks|(1u<<30)); return b;
}
static Bytes wrap(const Bytes& packet) { Bytes b; word(b,1); word(b,unsigned(packet.size())); b.insert(b.end(),packet.begin(),packet.end()); return b; }
int main() {
    PyroWave::Frame frame; std::string error;
    const PyroWave::StreamContext context{1920,1080,PyroWave::Chroma::Yuv420,true};
    auto parse=[&](const Bytes& b,bool expected,const PyroWave::StreamContext& ctx) {
        const bool result=PyroWave::parseLiveCompatibilityFrame(b.data(),b.size(),b.size(),frame,error,ctx);
        require(result==expected);
        require(expected ? error.empty() : !error.empty() && frame.packets.empty() && frame.records.empty());
    };
    const auto valid=wrap(sequence(0));
    parse(valid,true,context);
    for (std::size_t i=0;i<valid.size();++i) parse(Bytes(valid.begin(),valid.begin()+i),false,context);
    auto bad=valid; set(bad,0,0); parse(bad,false,context);
    bad=valid; set(bad,0,PyroWave::LiveLimits.packets+1); parse(bad,false,context);
    bad=valid; set(bad,4,0xffffffffu); parse(bad,false,context);
    bad=valid; set(bad,12,0); parse(bad,false,context); // Full-range metadata
    bad=valid; set(bad,12,(1u<<30)|(1u<<27)); parse(bad,false,context); // HDR primaries
    bad=valid; set(bad,12,(1u<<30)|(1u<<26)); parse(bad,false,context); // 444
    parse(wrap(sequence(0,1280,720)),false,context);
    parse(sequence(0),false,context); // Live record framing is excluded
    bad.assign(PyroWave::LiveLimits.frameBytes+1,0); parse(bad,false,context);
    parse(valid,true,context); // Recovery after every rejected independent frame
    require(PyroWave::validLiveExtent(3840,2160) && PyroWave::validLiveExtent(1920,1080));
    require(!PyroWave::validLiveExtent(1919,1080) && !PyroWave::validLiveExtent(4096,4096));

    // Real structurally valid 4K records, each in its own codec packet, exceeding
    // both offline limits. Interior 32x32 blocks carry 16 active 8x8 entries,
    // 18 zero coefficient planes per group; there are no signs or padding.
    std::vector<Bytes> packets{sequence(0,3840,2160)};
    unsigned base=0;
    for (int level=4;level>=0;--level) {
        unsigned x8=((3840u>>(level+1))+7)/8, y8=((2176u>>(level+1))+7)/8;
        unsigned x32=(x8+3)/4,y32=(y8+3)/4;
        unsigned groups=(level==0 ? 1 : 3)*(level==4 ? 4 : 3);
        for (unsigned group=0;group<groups;++group)
            for (unsigned y=0;y<y32;++y) for (unsigned x=0;x<x32;++x) {
                if (x8-4*x<4 || y8-4*y<4) continue;
                Bytes block; word(block,(590u<<16)|0xffffu); word(block,(base+group*x32*y32+y*x32+x)<<8);
                block.insert(block.end(),32,0xff); block.insert(block.end(),16,15);
                block.insert(block.end(),16*8*18,0);
                packets.push_back(std::move(block));
                if (packets.size()==1501) goto complete;
            }
        base+=groups*x32*y32;
    }
complete:
    require(packets.size()==1501);
    set(packets[0],4,1500u|(1u<<30));
    Bytes large; word(large,unsigned(packets.size()));
    for (const auto& p:packets) { word(large,unsigned(p.size())); large.insert(large.end(),p.begin(),p.end()); }
    const PyroWave::StreamContext fourK{3840,2160,PyroWave::Chroma::Yuv420,true};
    require(large.size()>PyroWave::MaxFrameBytes);
    parse(large,true,fourK);
    require(!PyroWave::parseFrame(large.data(),large.size(),large.size(),frame,error,&fourK));

    // Arbitrary common-c fragment cuts, including cuts inside lengths/records.
    for (unsigned cut=1;cut<valid.size();++cut) {
        LENTRY second{}; second.data=(char*)valid.data()+cut; second.length=int(valid.size()-cut); second.bufferType=BUFFER_TYPE_PICDATA;
        LENTRY first{}; first.data=(char*)valid.data(); first.length=cut; first.bufferType=BUFFER_TYPE_PICDATA; first.next=&second;
        DECODE_UNIT du{}; du.fullLength=int(valid.size()); du.bufferList=&first;
        Bytes bytes;
        require(PyroWave::assembleLiveDecodeUnit(du,bytes,error) && bytes==valid);
        parse(bytes,true,context);
        ++du.fullLength; require(!PyroWave::assembleLiveDecodeUnit(du,bytes,error) && bytes.empty());
        --du.fullLength; second.bufferType=BUFFER_TYPE_SPS;
        require(!PyroWave::assembleLiveDecodeUnit(du,bytes,error) && bytes.empty());
        second.bufferType=BUFFER_TYPE_PICDATA; second.next=&first;
        require(!PyroWave::assembleLiveDecodeUnit(du,bytes,error) && bytes.empty());
        second.next=nullptr;
    }
    parse(valid,true,context);
    std::cout << "PASS: live bounds, profiles, arbitrary DU fragments, malformed rejection/recovery; large=" << large.size() << "\n";
}
