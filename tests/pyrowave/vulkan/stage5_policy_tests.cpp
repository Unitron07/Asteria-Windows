#include "pyrowave_vulkan_live_policy.h"
#include <iostream>
#include <random>
#include <thread>
#include <vector>
using namespace PyroWaveVulkan;
static void require(bool condition) { if(!condition) throw std::runtime_error("Stage 5 policy assertion"); }
template<class F> void rejects(F operation) { bool rejected=false; try { operation(); } catch(const std::exception&) { rejected=true; } require(rejected); }
int main() {
    try {
        bool queued=false; unsigned publications=0;
        const auto overlayNotification=[&] { wakeFrame(queued,[&] { ++publications; return true; }); };
        for(unsigned i=0;i<1000;++i) overlayNotification();
        require(queued && publications==1); // Overlay-only updates coalesce without any decode.
        queued=false; overlayNotification(); require(queued && publications==2);
        queued=false; wakeFrame(queued,[&] { ++publications; return false; });
        require(!queued); overlayNotification(); require(queued && publications==4);
        for(bool retirementFails:{false,true}) for(bool drainFails:{false,true}) {
            std::vector<int> teardown;
            const auto run=[&] { retireAndDrain([&] {
                teardown.push_back(1); if(retirementFails) throw std::runtime_error("retirement failure");
            },[&] {
                teardown.push_back(2); if(drainFails) throw std::runtime_error("drain failure");
            }); };
            if(retirementFails || drainFails) rejects(run); else run();
            require(teardown==std::vector<int>({1,2}));
        }
        for(int successful=0;successful<3;++successful) {
            std::vector<int> attempts; BackendSelection selection;
            const auto attempt=[&](int i) { attempts.push_back(i); return i==successful; };
            const auto selected=selection.initialize([&] { return attempt(0); },[&] { return attempt(1); },[&] { return attempt(2); });
            require(int(selected)==successful && attempts.size()==size_t(successful+1));
            selection.beginLive(); selection.fatal();
            rejects([&] { selection.initialize([&] { return attempt(0); },[&] { return attempt(1); },[&] { return attempt(2); }); });
            require(attempts.size()==size_t(successful+1)); // No hot fallback attempt.
        }
        LiveSlots slots; std::array<Stage3::Payloads,3> timeline;
        uint64_t retirements=0,redraws=0,reuses=0; std::array<unsigned,3> decodes{};
        const auto retire=[&](int i) { require(timeline[i].pending); timeline[i].complete(timeline[i].decoded+1); ++retirements; };
        std::mt19937 random(0x51a075);
        for(unsigned n=0;n<200000;++n) {
            const unsigned operation=random()%5;
            if(operation<3) {
                const int protectedSlot=slots.displayed;
                const int i=slots.reserve(retire); require(i!=protectedSlot && i>=0 && i<3);
                if(decodes[i]++) ++reuses;
                const auto values=timeline[i].next(); require(values[0]<values[1] && values[1]<values[2]);
                if(random()%13==0) { // Malformed input after reservation.
                    slots.cancel(i); require(!timeline[i].pending); continue;
                }
                timeline[i].submitted(values[1]); slots.publish(i,retire);
            } else if(operation==3) {
                const int i=slots.current(); if(i<0) continue;
                if(!timeline[i].pending) { timeline[i].decoded=timeline[i].consumed; timeline[i].pending=true; ++redraws; }
                timeline[i].complete(timeline[i].decoded+1); slots.presented(i);
            } else slots.suspend(retire); // Minimized newest-frame handling.
            unsigned displayed=0,pending=0,decoding=0;
            for(auto state:slots.states) { displayed+=state==LiveSlots::State::Displayed; pending+=state==LiveSlots::State::Pending; decoding+=state==LiveSlots::State::Decode; }
            require(displayed<=1 && pending<=1 && !decoding);
        }
        require(retirements>10000 && redraws>1000 && reuses>10000);
        LiveSlots failed; const int first=failed.reserve([](int) {}); failed.publish(first,[](int) {});
        const int second=failed.reserve([](int) {});
        rejects([&] { failed.publish(second,[](int) { throw std::runtime_error("GPU retirement failure"); }); });
        require(failed.pending==first && failed.states[first]==LiveSlots::State::Pending && failed.states[second]==LiveSlots::State::Decode);
        Stage3::Payloads overflow; overflow.consumed=UINT64_MAX-1; rejects([&] { overflow.next(); });
        Stage3::QueueLock queue;
        { Stage3::QueueLock::Guard lock(queue); rejects([&] { queue.lock(true); }); }
        std::thread decoder([&] { for(int i=0;i<10000;++i) { queue.lock(true); queue.unlock(true); } });
        for(int i=0;i<10000;++i) { Stage3::QueueLock::Guard lock(queue); }
        decoder.join(); require(queue.balanced());
        require(overlayBytes(31,17)==31*17*4); rejects([&] { overlayBytes(4096,4096); });
        require(overlayRect(500,70,720,false).y==0 && overlayRect(500,70,720,true).y==650);
        std::cout<<"PASS Stage 5: overlay-only wake coalescing/retry; fallback/no hot switch; 200000 bounded slot operations; retirement failure retains state; redraws="<<redraws<<" drops="<<retirements<<" reuses="<<reuses<<"; overflow; cross-thread nonrecursive queue lock\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
