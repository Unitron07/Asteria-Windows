#pragma once
#include "pyrowave_frame.h"
#include <SDL.h>

namespace PyroWave {
// Only for the pinned v15 SDL2-compat/SDL3 build. Renderer/texture pointers
// pass through unchanged in that compat layer. No global YUV conversion mode.
SDL_Texture* createLiveTexture(SDL_Renderer* renderer, int width, int height, YuvRange range, void* const* d3dPlanes = nullptr);
void* rendererD3D11Device(SDL_Renderer* renderer);
}
