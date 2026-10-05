#include "pyrowave_sdl.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace PyroWave {
SDL_Texture* createLiveTexture(SDL_Renderer* renderer, int width, int height, YuvRange range, void* const* d3dPlanes) {
    // SDL3 is already loaded and owned by SDL2-compat. Resolve from that exact
    // module; do not import SDL3 symbols under the colliding SDL2 API names.
    SDL_version version{};
    SDL_GetVersion(&version);
    if (version.major != 2 || version.minor != 32 || version.patch != 74) {
        SDL_SetError("PyroWave colorspace requires the pinned v19 SDL2-compat build");
        return nullptr;
    }
    const auto module = GetModuleHandleW(L"SDL3.dll");
    if (!module) { SDL_SetError("PyroWave colorspace requires loaded SDL3"); return nullptr; }
    using CreateProperties = Uint32 (SDLCALL*)();
    using SetNumberProperty = bool (SDLCALL*)(Uint32, const char*, Sint64);
    using SetPointerProperty = bool (SDLCALL*)(Uint32, const char*, void*);
    using DestroyProperties = void (SDLCALL*)(Uint32);
    using CreateTexture = SDL_Texture* (SDLCALL*)(SDL_Renderer*, Uint32);
    const auto create = reinterpret_cast<CreateProperties>(GetProcAddress(module,"SDL_CreateProperties"));
    const auto set = reinterpret_cast<SetNumberProperty>(GetProcAddress(module,"SDL_SetNumberProperty"));
    const auto setPointer = reinterpret_cast<SetPointerProperty>(GetProcAddress(module,"SDL_SetPointerProperty"));
    const auto destroy = reinterpret_cast<DestroyProperties>(GetProcAddress(module,"SDL_DestroyProperties"));
    const auto texture = reinterpret_cast<CreateTexture>(GetProcAddress(module,"SDL_CreateTextureWithProperties"));
    if (!create || !set || !destroy || !texture) {
        SDL_SetError("PyroWave SDL3 texture/colorspace API unavailable"); return nullptr;
    }
    // Exact SDL_Colorspace values and property names from pinned SDL3 commit
    // 829a65d769d935c4852f8159e964312c0957260a, SDL_pixels.h / SDL_render.h.
    constexpr Sint64 Bt709Limited = 0x21100421u, Bt709Full = 0x22100421u;
    const auto props = create();
    if (!props) return nullptr;
    bool ok = set(props,"SDL.texture.create.format",SDL_PIXELFORMAT_IYUV) &&
        set(props,"SDL.texture.create.access",d3dPlanes ? SDL_TEXTUREACCESS_STATIC : SDL_TEXTUREACCESS_STREAMING) &&
        set(props,"SDL.texture.create.width",width) &&
        set(props,"SDL.texture.create.height",height) &&
        set(props,"SDL.texture.create.colorspace",range == YuvRange::Full ? Bt709Full : Bt709Limited);
    if (d3dPlanes) {
        ok = ok && setPointer &&
            setPointer(props,"SDL.texture.create.d3d11.texture",d3dPlanes[0]) &&
            setPointer(props,"SDL.texture.create.d3d11.texture_u",d3dPlanes[1]) &&
            setPointer(props,"SDL.texture.create.d3d11.texture_v",d3dPlanes[2]);
    }
    auto result = ok ? texture(renderer,props) : nullptr;
    destroy(props);
    if (result && (SDL_SetTextureBlendMode(result,SDL_BLENDMODE_NONE) != 0 ||
                   SDL_SetTextureScaleMode(result,SDL_ScaleModeLinear) != 0)) {
        SDL_DestroyTexture(result); return nullptr;
    }
    return result;
}
void* rendererD3D11Device(SDL_Renderer* renderer) {
    const auto module = GetModuleHandleW(L"SDL3.dll");
    if (!module) return nullptr;
    using Properties = Uint32 (SDLCALL*)(SDL_Renderer*);
    using GetPointer = void* (SDLCALL*)(Uint32,const char*,void*);
    const auto properties = reinterpret_cast<Properties>(GetProcAddress(module,"SDL_GetRendererProperties"));
    const auto get = reinterpret_cast<GetPointer>(GetProcAddress(module,"SDL_GetPointerProperty"));
    return properties && get ? get(properties(renderer),"SDL.renderer.d3d11.device",nullptr) : nullptr;
}
}
