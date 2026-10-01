#include "pyrowave_frame.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
using Bytes = std::vector<std::uint8_t>;
static int cases = 0;
static void require(bool ok, const char* name) {
    if (!ok) { std::cerr << "FAIL: " << name << '\n'; std::exit(1); }
}
static void word(Bytes& b, std::uint32_t n) {
    for (unsigned shift : {0,8,16,24}) b.push_back(std::uint8_t(n >> shift));
}
static void set(Bytes& b, std::size_t offset, std::uint32_t n) {
    for (unsigned i=0;i<4;++i) b[offset+i]=std::uint8_t(n>>(8*i));
}
static Bytes compatibility(std::initializer_list<Bytes> packets) {
    Bytes b; word(b,std::uint32_t(packets.size()));
    for (const auto& p : packets) { word(b,std::uint32_t(p.size())); b.insert(b.end(),p.begin(),p.end()); }
    return b;
}
static Bytes sequence(std::uint32_t count=2, unsigned w=1920, unsigned h=1080,
                      unsigned chroma=0, unsigned seq=3) {
    Bytes b; word(b,0x80000000u|(seq<<28)|((h-1)<<14)|(w-1)); word(b,count|(chroma<<26)); return b;
}
static void block(Bytes& b, unsigned index, unsigned words=3, unsigned seq=3) {
    word(b,(seq<<28)|(words<<16)|1); word(b,index<<8); // one active 8x8, zero planes
    for (unsigned i=2;i<words;++i) word(b,0);
}
static Bytes records() { auto b=sequence(); block(b,12); block(b,0); return b; }
static void test(const char* name, const Bytes& b, bool valid,
                 const PyroWave::StreamContext* context=nullptr,
                 std::size_t declared=std::numeric_limits<std::size_t>::max()) {
    PyroWave::Frame f; f.packets.push_back({999,999}); f.records.push_back({PyroWave::RecordKind::Block,999,999,999});
    f.payloadBytes=999; std::string error;
    const auto length=declared==std::numeric_limits<std::size_t>::max()?b.size():declared;
    const bool ok=PyroWave::parseFrame(b.data(),b.size(),length,f,error,context);
    require(ok==valid,name);
    require(valid ? error.empty()&&!f.packets.empty() :
            !error.empty()&&f.packets.empty()&&f.records.empty()&&f.payloadBytes==0&&f.sequence.width==0,name);
    if (ok) {
        std::size_t sum=0;
        for (const auto& p:f.packets) { require(p.offset<=b.size()&&p.size<=b.size()-p.offset,name); sum+=p.size; }
        require(sum==f.payloadBytes,name);
    }
    ++cases;
}
int main() {
    const PyroWave::StreamContext sdr{1920,1080,PyroWave::Chroma::Yuv420,true};
    const auto one=compatibility({{1,2,3}}), multi=compatibility({{1},{2,3},{4,5,6}});
    test("LE one packet",one,true); test("LE multiple packets",multi,true);
    auto b=one; set(b,0,0); test("zero count",b,false);
    b=one; set(b,0,PyroWave::MaxPackets+1); test("count limit",b,false);
    b=one; set(b,0,0x7fffffffu); test("pathological count",b,false);
    b=one; set(b,4,0); test("zero length",b,false);
    b=one; set(b,4,0xffffffffu); test("UINT32_MAX length",b,false);
    b=one; set(b,4,0x80000000u); test("high-bit length",b,false);
    b=multi; set(b,0,2); test("count too low",b,false);
    b=multi; set(b,0,4); test("count too high",b,false);
    b=one; b.push_back(0); test("trailing byte",b,false);
    b=compatibility({Bytes(PyroWave::MaxFrameBytes-8,42)}); test("exact frame limit",b,true);
    b.push_back(0); test("over frame limit",b,false);
    test("declared mismatch",one,false,nullptr,one.size()+1);
    test("huge declared size",one,false,nullptr,std::numeric_limits<std::size_t>::max()-1);
    for (std::size_t n=0;n<one.size();++n) test("compatibility truncation",Bytes(one.begin(),one.begin()+n),false);
    Bytes maxPackets; word(maxPackets,PyroWave::MaxPackets);
    for (std::size_t i=0;i<PyroWave::MaxPackets;++i) { word(maxPackets,1); maxPackets.push_back(42); }
    test("exact packet limit",maxPackets,true);
    auto r=records(); test("records out of index order",r,true,&sdr);
    PyroWave::Frame f; std::string error;
    require(PyroWave::parseFrame(r.data(),r.size(),r.size(),f,error,&sdr),"record offsets");
    require(f.framing==PyroWave::Framing::Records&&f.packets.size()==3&&f.packets[1].offset==8&&
            f.packets[1].size==12&&f.sequence.number==3&&f.sequence.blockCapacity==3261,"record model and geometry");
    auto seq=sequence(); Bytes a; block(a,12); Bytes c; block(c,0);
    b=compatibility({seq,a,c}); test("compatibility codec validation",b,true,&sdr);
    test("opaque framing alone",one,true); test("opaque data rejected by runtime context",one,false,&sdr);
    b=r; set(b,8,(4u<<28)|(3u<<16)|1); test("sequence mismatch",b,false);
    b=r; set(b,12,3261u<<8); test("index capacity bound",b,false);
    b=r; set(b,24,12u<<8); test("duplicate indices",b,false);
    b=r; set(b,8,(3u<<28)|1); test("zero payload words",b,false);
    b=r; set(b,8,(3u<<28)|(1u<<16)|1); test("short payload words",b,false);
    b=r; set(b,8,(3u<<28)|(2u<<16)|1); test("conditional zero block",b,false);
    b=r; set(b,8,(3u<<28)|(4095u<<16)|1); test("record runs off frame",b,false);
    b=r; set(b,4,1); test("excess records",b,false);
    b=r; set(b,4,3); test("missing records",b,false);
    b=r; set(b,4,0xffffffu); test("pathological block count",b,false);
    b=r; set(b,4,0); test("zero blocks",b,false);
    b=r; set(b,4,2|(1u<<24)); test("keep previous sequence rejected",b,false);
    b=r; set(b,4,2|(1u<<26)); test("negotiated chroma mismatch",b,false,&sdr);
    b=r; set(b,4,2|(1u<<28)); test("HDR excluded in P0",b,false,&sdr);
    b=sequence(2,1919); block(b,12); block(b,0); test("negotiated width mismatch",b,false,&sdr);
    b=sequence(2,1920,1079); block(b,12); block(b,0); test("negotiated height mismatch",b,false,&sdr);
    b=sequence(); b.insert(b.end(),r.begin(),r.end()); test("second sequence",b,false);
    b.assign(r.begin()+8,r.end()); test("block before sequence",b,false);
    b=sequence(); word(b,0xffffffffu); word(b,2); word(b,0); word(b,0); block(b,0);
    word(b,0xffffffffu); word(b,0); block(b,12);
    test("padding between records including zero words",b,true,&sdr);
    require(PyroWave::parseFrame(b.data(),b.size(),b.size(),f,error,&sdr)&&f.records.size()==5&&f.packets.size()==3&&
            f.payloadBytes==32,"padding omitted from C API spans");
    auto padded=b; set(b,16,1); test("nonzero padding",b,false);
    b=padded; set(b,12,0xffffffffu); test("padding overflow",b,false);
    b=padded; b.erase(b.begin(),b.begin()+8); test("padding before sequence",b,false);
    b=r; b.push_back(0); test("record trailing bytes",b,false);
    for (std::size_t n=0;n<r.size();++n) test("every record truncation",Bytes(r.begin(),r.begin()+n),false);
    b={'P','Y','R','W',1,0,1,0,0,0,0,1,42}; test("legacy never auto detected",b,false);
    require(PyroWave::parseLegacyOfflineFrame(b.data(),b.size(),b.size(),f,error)&&f.framing==PyroWave::Framing::LegacyOffline,"explicit legacy fixture");
    require(!PyroWave::parseFrame(nullptr,8,8,f,error),"null input");
    require(!PyroWave::parseFrame(r.data(),std::numeric_limits<std::size_t>::max(),std::numeric_limits<std::size_t>::max(),f,error),"size overflow before read");
    // Complete-frame parsing sees reassembled bytes, so arbitrary transport cuts
    // through headers, payloads and padding must not change offsets or results.
    for (std::size_t cut=1;cut<padded.size();++cut) {
        b.assign(padded.begin(),padded.begin()+cut); b.insert(b.end(),padded.begin()+cut,padded.end());
        test("record straddles transport boundary",b,true,&sdr);
    }
    Bytes unaligned(r.size()+1); std::copy(r.begin(),r.end(),unaligned.begin()+1);
    require(PyroWave::parseFrame(unaligned.data()+1,r.size(),r.size(),f,error,&sdr),"unaligned byte input");
    std::uint32_t seed=12345;
    for (int i=0;i<20000;++i) {
        b=(i&1)?multi:r; seed=seed*1664525u+1013904223u; b[seed%b.size()]^=std::uint8_t(seed>>24);
        if (!PyroWave::parseFrame(b.data(),b.size(),b.size(),f,error))
            require(f.packets.empty()&&f.records.empty()&&f.payloadBytes==0,"mutations clear state");
    }
    std::cout<<"PASS: "<<cases<<" framing cases + 20000 bounded mutations\n";
}
