#pragma once
#include "pyrowave_runtime.h"
#include <SDL.h>
#include <memory>

namespace PyroWave {
// D3D-owned R8 planes imported into Vulkan. No CPU video staging or re-upload.
// SDL and D3D immediate-context methods must run on the main thread.
class GpuPresentation {
public:
    explicit GpuPresentation(Runtime& runtime);
    ~GpuPresentation();
    bool initialize(SDL_Renderer* renderer, int width, int height);
    bool decode(int slot, const std::vector<uint8_t>& bytes, std::size_t& packets, DecodeTiming& timing);
    SDL_Texture* texture(int slot);
    bool beginRender(int slot);
    bool endRender(int slot);
    const std::string& error() const;
private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};
}
