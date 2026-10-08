#pragma once
#include "pyrowave_vulkan_shared_policy.h"
#include <functional>
#include <optional>

namespace PyroWaveVulkan {
// Used by decode, overlay-only and window notifications under the decoder mutex.
// A failed SDL publication must remain retryable, without another video frame.
template<class Publish> void wakeFrame(bool& queued,Publish publish) {
    if(!queued) queued=publish();
}
enum class Backend { Native, Legacy, Cpu };
// Initialization only. A selected backend never hot-switches after packets start.
template<class Native,class Legacy,class Cpu>
Backend selectBackend(Native native,Legacy legacy,Cpu cpu) {
    if(native()) return Backend::Native;
    if(legacy()) return Backend::Legacy;
    if(cpu()) return Backend::Cpu;
    throw std::runtime_error("all PyroWave presentation initialization paths failed");
}
class BackendSelection {
    std::optional<Backend> chosen;
    bool live=false,failed=false;
public:
    template<class N,class L,class C> Backend initialize(N native,L legacy,C cpu) {
        if(live || failed) throw std::runtime_error("PyroWave backend fallback forbidden after live/fatal state");
        chosen=selectBackend(native,legacy,cpu); return *chosen;
    }
    void beginLive() {
        if(!chosen || failed) throw std::runtime_error("live packet before successful presentation initialization");
        live=true;
    }
    void fatal() { failed=true; }
};
// Externally serialized. Every retirement must submit a GPU wait/signal before
// this policy releases the slot; exceptions retain state for fatal teardown.
class LiveSlots {
public:
    static constexpr unsigned Count=3;
    enum class State { Free, Decode, Pending, Displayed };
    std::array<State,Count> states{};
    int pending=-1,displayed=-1;
    template<class Retire> int reserve(Retire retire) {
        for(unsigned i=0;i<Count;++i) if(states[i]==State::Free) {
            states[i]=State::Decode; return int(i);
        }
        if(pending>=0) {
            const int i=pending; retire(i); pending=-1; states[i]=State::Decode; return i;
        }
        throw std::runtime_error("native three-slot reservation invariant");
    }
    template<class Retire> void publish(int i,Retire retire) {
        if(states.at(i)!=State::Decode) throw std::runtime_error("publish without reservation");
        if(pending>=0) { retire(pending); states[pending]=State::Free; }
        pending=i; states[i]=State::Pending;
    }
    void cancel(int i) {
        if(states.at(i)!=State::Decode) throw std::runtime_error("cancel without reservation");
        states[i]=State::Free;
    }
    int current() const { return pending>=0 ? pending : displayed; }
    void presented(int i) {
        if(i!=current()) throw std::runtime_error("presenting a stale native slot");
        if(i==pending) {
            if(displayed>=0) states[displayed]=State::Free;
            pending=-1; displayed=i; states[i]=State::Displayed;
        }
    }
    template<class Retire> void suspend(Retire retire) {
        if(pending>=0) { retire(pending); states[pending]=State::Free; pending=-1; }
    }
};
struct OverlayRect { int x,y,width,height; };
inline OverlayRect overlayRect(int width,int height,int drawableHeight,bool status) {
    if(width<=0 || height<=0) throw std::runtime_error("invalid overlay extent");
    return {0,status ? drawableHeight-height : 0,width,height};
}
// SDL_Surface updates and uploads are bounded independently of video resolution.
constexpr size_t MaxOverlayBytes=16*1024*1024;
inline size_t overlayBytes(int width,int height) {
    if(width<=0 || height<=0 || uint64_t(width)*height>MaxOverlayBytes/4)
        throw std::runtime_error("overlay exceeds bounded RGBA upload size");
    return size_t(width)*height*4;
}
}
