#include "shared_policy.h"
#include <iostream>
using namespace Stage3;
static void require(bool ok) { if (!ok) throw std::runtime_error("shared-device policy failure"); }
template<class F> static void rejects(F f) { bool rejected=false; try { f(); } catch (const std::exception&) { rejected=true; } require(rejected); }
int main() {
    try {
        auto p=planes(1920,1080);
        require(p[0].bytes==2073600 && p[1].bytes==518400 && p[2].width==960 && p[2].height==540);
        rejects([] { planes(1919,1080); }); rejects([] { planes(UINT32_MAX,1080); });
        require(usage(Path::Compute)==9 && usage(Path::Fragment)==17 && SlotCount==3);
        Capabilities c{true,true,true,true,true,true,true,true,false,true};
        require(supported(c)); c.storage8=false; require(!supported(c)); c.largeTexelBuffers=true; require(supported(c));
        for (int n=0;n<8;++n) {
            auto missing=c;
            switch(n) { case 0: missing.api12=false;break; case 1:missing.timeline=false;break;
                case 2:missing.sync2=false;break;case 3:missing.subgroupSizeControl=false;break;
                case 4:missing.computeFullSubgroups=false;break;case 5:missing.subgroupRange=false;break;
                case 6:missing.subgroupOperations=false;break;case 7:missing.writeWithoutFormat=false;break; }
            require(!supported(missing));
        }
        require(identical(1,2,3,1,2,3));
        require(!identical(1,2,3,1,2,4) && !identical(1,2,3,1,4,3) && !identical(1,2,3,4,2,3));
        std::array<Payloads,SlotCount> slots;
        for (unsigned n=0;n<60;++n) {
            auto& s=slots[n%SlotCount]; auto values=s.next();
            require(values[0]>0 && values[0]<values[1] && values[1]<values[2]);
            rejects([&] { s.submitted(0); }); s.submitted(values[1]);
            rejects([&] { s.next(); }); rejects([&] { s.complete(values[1]); });
            s.complete(values[2]); require(s.next()[0]==values[2]);
        }
        slots[0].consumed=UINT64_MAX; rejects([&] { slots[0].next(); });
        QueueLock queue;
        { QueueLock::Guard guard(queue); rejects([&] { queue.lock(true); }); }
        queue.lock(true); queue.unlock(true);
        rejects([&] { queue.unlock(true); });
        std::thread t([&] { queue.lock(true); queue.unlock(true); }); t.join();
        require(queue.balanced() && queue.codecLocks==2 && queue.callerLocks==1);
        std::cout<<"PASS: GPU-free requirements, usages, planes, three slots, real payload bookkeeping, handle identity and queue locks\n";
        return 0;
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
