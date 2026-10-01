#define SDL_MAIN_HANDLED
#include "pyrowave_sdl.h"
#include <array>
#include <cstdlib>
#include <iostream>

static void require(bool ok) {
    if (!ok) { std::cerr << "FAIL live SDL range: " << SDL_GetError() << '\n'; std::exit(1); }
}
int main() {
    SDL_SetMainReady();
    require(SDL_Init(SDL_INIT_VIDEO)==0);
    auto window=SDL_CreateWindow("PyroWave range regression",0,0,128,128,SDL_WINDOW_HIDDEN);
    require(window!=nullptr);
    auto renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
    require(renderer!=nullptr);
    std::array<Uint8,128*128> y{};
    std::array<Uint8,64*64> u{},v{};
    std::array<Uint32,128*128> rgb{};
    auto format=SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888);
    require(format!=nullptr);
    // Read rendered RGB, not just the supplied property: limited 16/235 map
    // to 0/255, full 16/235 remain 16/235. This catches accepting full but
    // accidentally retaining SDL2's global limited-range conversion mode.
    SDL_SetYUVConversionMode(SDL_YUV_CONVERSION_BT709);
    for (const auto range : {PyroWave::YuvRange::Limited,PyroWave::YuvRange::Full}) {
        auto texture=PyroWave::createLiveTexture(renderer,128,128,range);
        require(texture!=nullptr);
        u.fill(128); v.fill(128);
        for (const int value : {16,235}) {
            y.fill(Uint8(value));
            require(SDL_UpdateYUVTexture(texture,nullptr,y.data(),128,u.data(),64,v.data(),64)==0);
            require(SDL_RenderClear(renderer)==0 && SDL_RenderCopy(renderer,texture,nullptr,nullptr)==0);
            require(SDL_RenderReadPixels(renderer,nullptr,SDL_PIXELFORMAT_ARGB8888,rgb.data(),128*4)==0);
            Uint8 r,g,b; SDL_GetRGB(rgb[64*128+64],format,&r,&g,&b);
            const int expected=range==PyroWave::YuvRange::Full ? value : value==16 ? 0 : 255;
            require(std::abs(int(r)-expected)<=2 && std::abs(int(g)-expected)<=2 && std::abs(int(b)-expected)<=2);
        }
        // Non-neutral chroma also distinguishes BT.709 full from JPEG/BT.601
        // full (which would render approximately 238,14,14 for this triplet).
        y.fill(81); u.fill(90); v.fill(240);
        require(SDL_UpdateYUVTexture(texture,nullptr,y.data(),128,u.data(),64,v.data(),64)==0);
        require(SDL_RenderClear(renderer)==0 && SDL_RenderCopy(renderer,texture,nullptr,nullptr)==0);
        require(SDL_RenderReadPixels(renderer,nullptr,SDL_PIXELFORMAT_ARGB8888,rgb.data(),128*4)==0);
        Uint8 r,g,b; SDL_GetRGB(rgb[64*128+64],format,&r,&g,&b);
        require(std::abs(int(r)-255)<=2);
        require(std::abs(int(g)-(range==PyroWave::YuvRange::Full ? 36 : 24))<=2);
        require(std::abs(int(b)-(range==PyroWave::YuvRange::Full ? 10 : 0))<=2);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeFormat(format);
    SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
    std::cout << "PASS: BT.709 full/limited SDL3 textures render correct RGB endpoints\n";
}
