#include "pyrowave_perf.h"
#include <cstdlib>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>
using namespace PyroWavePerf;
#define CHECK(x) do { if(!(x)) { std::cerr << __LINE__ << ": " << #x << '\n'; std::exit(1); } } while(0)
int main() {
    Histogram h;
    CHECK(h.count==0);
    for(uint64_t v=0; v<100000; ++v) {
        const auto bound=upper(bucket(v));
        CHECK(bound>=v); CHECK(bound-v<=v/8);
        h.record(v);
    }
    CHECK(h.count==100000); CHECK(h.sum==4999950000ULL);
    CHECK(h.minimum==0); CHECK(h.maximum==99999);
    for(auto p:{50u,95u,99u,100u}) {
        const auto exact=1000*p-1;
        CHECK(h.percentile(p)>=exact); CHECK(h.percentile(p)-exact<=exact/8);
    }
    CHECK(upper(bucket(Max))==Max);
    Histogram small; for(auto v:{0,1,2,3,4,5,6,7}) small.record(uint64_t(v));
    CHECK(small.percentile(50)==3); CHECK(small.percentile(95)==7);
    Histogram overflow; overflow.sum=Max-1; overflow.record(2); CHECK(overflow.overflow); CHECK(overflow.sum==Max);
    std::atomic<uint64_t> counter{Max-2}; CHECK(add(counter,2)); CHECK(!add(counter,1)); CHECK(counter==Max);
    auto c=std::make_unique<Capture>();
    c->start=nowUs(); c->warmup=0; c->duration=100000000;
    CHECK(!c->active(c->start-1)); CHECK(c->active(c->start)); CHECK(!c->active(c->start+c->duration));
    c->ordered(Assembly,9,10); CHECK(c->counts[InvalidTimestamps]==1); CHECK(c->metrics[Assembly].count==0);
    c->ordered(Assembly,2000,1000); CHECK(c->metrics[Assembly].sum==1000); // us, not ms
    c->submitted(true,true,10,2); c->submitted(false,false,10,2); c->submitted(false,true,10,2);
    CHECK(c->counts[Submitted]==1); CHECK(c->counts[RetainedRedraws]==2); CHECK(c->counts[OverlayRedraws]==1);
    c->received(0xfffffffeu); c->received(1); CHECK(c->counts[NetworkDrops]==2);
    c->received(1); CHECK(c->counts[NetworkDrops]==2);
    c->received(0); c->received(2); CHECK(c->counts[NetworkDrops]==2);
    for(size_t i=0;i<Capture::EventCapacity*2;++i) c->event(Drop,uint32_t(i),1);
    CHECK(c->eventCount>Capture::EventCapacity); CHECK(c->events.front().stage==Submit);
    c->eventCount=Max; c->event(Drop,1); CHECK(c->eventCount==Max); CHECK(c->counterOverflow);
    const auto storage=sizeof(*c);
    for(int i=0;i<1000000;++i) c->metrics[Preparation].record(uint64_t(i%10000));
    CHECK(sizeof(*c)==storage); CHECK(c->metrics[Preparation].count==1000000);
    std::vector<std::thread> threads;
    for(int i=0;i<4;++i) threads.emplace_back([&] { for(int j=0;j<10000;++j) c->metrics[RenderLoop].record(5); });
    for(auto& thread:threads) thread.join();
    CHECK(c->metrics[RenderLoop].count==40000); CHECK(c->metrics[RenderLoop].sum==200000);
    c->warmup=100; CHECK(!c->active(c->start+99)); CHECK(c->active(c->start+100));
    c->end=nowUs(); CHECK(!c->active(c->end));
    std::cout << "PASS histogram bounds/nearest rank/counts/units/ordering/overflow/missing/bounded storage/classification/concurrent writers\n";
}
