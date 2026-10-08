#pragma once
#include "pyrowave_runtime.h"
#include <SDL.h>
#include <memory>

namespace PyroWave {
class NativePresentation {
public:
    struct Diagnostics {
        uint64_t retiredDrops=0,presentationDrops=0,slotReuse=0,decodeWaits=0,consumerSignals=0;
        uint64_t recreations=0,overlayUploads=0,overlayRedraws=0,fatalErrors=0,cpuReadbacks=0;
        uint64_t decodeSubmitUs=0,renderLoopUs=0,queueSubmitUs=0,presentCallUs=0;
        uint64_t decoded=0,rendered=0,timelineErrors=0;
        unsigned validationErrors=0,validationWarnings=0;
        bool validationActive=false,borrowedMatch=false,cleanupOkay=true;
        std::string gpu,preferredPath,actualPath;
    } diagnostics;
    struct RenderResult { bool newFrame=false,retry=false; uint64_t readyUs=0; };
    explicit NativePresentation(Runtime& runtime);
    ~NativePresentation();
    bool initialize(SDL_Window* window,int width,int height,bool vsync);
    bool decode(const std::vector<uint8_t>& bytes,size_t& packets,DecodeTiming& timing);
    RenderResult render();
    void updateOverlay(unsigned index,SDL_Surface* surface,bool enabled);
    void windowChanged();
    void shutdown() noexcept;
    void fatal(const std::string& reason);
    uint64_t presentationDrops() const;
    const std::string& error() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
