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
    require(frame.sequence.range==PyroWave::YuvRange::Limited);
    for (std::size_t i=0;i<valid.size();++i) parse(Bytes(valid.begin(),valid.begin()+i),false,context);
    auto bad=valid; set(bad,0,0); parse(bad,false,context);
    bad=valid; set(bad,0,PyroWave::LiveLimits.packets+1); parse(bad,false,context);
    bad=valid; set(bad,4,0xffffffffu); parse(bad,false,context);
    // Exact Vibepollo regression: bit 30 clear is valid full-range BT.709 SDR.
    auto full=valid; set(full,12,0); parse(full,true,context);
    require(frame.sequence.range==PyroWave::YuvRange::Full);
    require(error.find("live P1a requires BT.709 limited range")==std::string::npos);
    for (const auto range : {0u,1u<<30}) {
        for (unsigned bit=27;bit<=29;++bit) { // BT.2020 primaries, PQ transfer, BT.2020 matrix
            bad=valid; set(bad,12,range|(1u<<bit)); parse(bad,false,context);
        }
        bad=valid; set(bad,12,range|(1u<<26)); parse(bad,false,context); // 444
        for (unsigned code=1;code<=3;++code) {
            bad=valid; set(bad,12,range|(code<<24)); parse(bad,false,context);
        }
        bad=valid; set(bad,12,range|1u); parse(bad,false,context); // Missing declared block
        const auto duplicate=sequence(0);
        bad=valid; bad.insert(bad.end(),duplicate.begin(),duplicate.end());
        set(bad,4,16); set(bad,12,range); parse(bad,false,context); // Duplicate sequence
    }
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
    const auto packetStorage=frame.packets.data(), packetEnd=frame.packets.data()+frame.packets.size();
    const auto recordStorage=frame.records.data();
    const auto indexStorage=frame.blockIndices.data();
    const auto packetCapacity=frame.packets.capacity(), recordCapacity=frame.records.capacity(), indexCapacity=frame.blockIndices.capacity();
    const auto largePackets=frame.packets;
    for (int repeat=0;repeat<4;++repeat) {
        parse(valid,true,context);
        bad=large; set(bad,4,0xffffffffu); parse(bad,false,fourK);
        parse(large,true,fourK);
        require(frame.packets.data()==packetStorage && frame.packets.data()+frame.packets.size()==packetEnd);
        require(frame.records.data()==recordStorage && frame.blockIndices.data()==indexStorage);
        require(frame.packets.capacity()==packetCapacity && frame.records.capacity()==recordCapacity && frame.blockIndices.capacity()==indexCapacity);
        for (std::size_t i=0;i<largePackets.size();++i)
            require(frame.packets[i].offset==largePackets[i].offset && frame.packets[i].size==largePackets[i].size);
        require(frame.blockIndices.size()==1500);
    }
    require(!PyroWave::parseFrame(large.data(),large.size(),large.size(),frame,error,&fourK));

    // Arbitrary common-c fragment cuts, including cuts inside lengths/records.
    Bytes bytes;
    bytes.reserve(large.size());
    const auto assemblyStorage=bytes.data();
    const auto assemblyCapacity=bytes.capacity();
    for (const auto& fixture : {valid,full,large}) for (unsigned cut=1;cut<std::min<std::size_t>(fixture.size(),32);++cut) {
        LENTRY second{}; second.data=(char*)fixture.data()+cut; second.length=int(fixture.size()-cut); second.bufferType=BUFFER_TYPE_PICDATA;
        LENTRY first{}; first.data=(char*)fixture.data(); first.length=cut; first.bufferType=BUFFER_TYPE_PICDATA; first.next=&second;
        DECODE_UNIT du{}; du.fullLength=int(fixture.size()); du.bufferList=&first;
        require(PyroWave::assembleLiveDecodeUnit(du,bytes,error) && bytes==fixture);
        require(bytes.data()==assemblyStorage && bytes.capacity()==assemblyCapacity);
        parse(bytes,true,fixture.size()==large.size() ? fourK : context);
        ++du.fullLength; require(!PyroWave::assembleLiveDecodeUnit(du,bytes,error) && bytes.empty());
        --du.fullLength; second.bufferType=BUFFER_TYPE_SPS;
        require(!PyroWave::assembleLiveDecodeUnit(du,bytes,error) && bytes.empty());
        second.bufferType=BUFFER_TYPE_PICDATA; second.next=&first;
        require(!PyroWave::assembleLiveDecodeUnit(du,bytes,error) && bytes.empty());
        second.next=nullptr;
        second.data=nullptr; require(!PyroWave::assembleLiveDecodeUnit(du,bytes,error) && bytes.empty());
        second.data=(char*)fixture.data()+cut;
        second.length=0; require(!PyroWave::assembleLiveDecodeUnit(du,bytes,error) && bytes.empty());
        second.length=-1; require(!PyroWave::assembleLiveDecodeUnit(du,bytes,error) && bytes.empty());
        second.length=int(fixture.size()-cut);
        du.fullLength=int(PyroWave::LiveLimits.frameBytes+1);
        require(!PyroWave::assembleLiveDecodeUnit(du,bytes,error) && bytes.empty());
        du.fullLength=0; require(!PyroWave::assembleLiveDecodeUnit(du,bytes,error) && bytes.empty());
        du.fullLength=int(fixture.size());
        require(PyroWave::assembleLiveDecodeUnit(du,bytes,error) && bytes==fixture && bytes.capacity()==assemblyCapacity);
    }
    std::vector<LENTRY> fragments(4001);
    char byte=0;
    for (std::size_t i=0;i<fragments.size();++i) {
        fragments[i].data=&byte; fragments[i].length=1; fragments[i].bufferType=BUFFER_TYPE_PICDATA;
        fragments[i].next=i+1<fragments.size() ? &fragments[i+1] : nullptr;
    }
    DECODE_UNIT du{}; du.fullLength=4001; du.bufferList=fragments.data();
    require(!PyroWave::assembleLiveDecodeUnit(du,bytes,error) && bytes.empty());
    fragments[3999].next=nullptr; du.fullLength=4000;
    require(PyroWave::assembleLiveDecodeUnit(du,bytes,error) && bytes==Bytes(4000,0));
    require(bytes.capacity()==assemblyCapacity);
    parse(valid,true,context);
    std::cout << "PASS: live bounds, profiles, arbitrary DU fragments, malformed rejection/recovery; large=" << large.size() << "\n";
}
