#define SDL_MAIN_HANDLED
#include "pyrowave_vulkan_probe.h"
#include <SDL_syswm.h>
#include <SDL_vulkan.h>
#include <fstream>
#include <iostream>
#include <mutex>
#include <memory>
#include <stdexcept>

using namespace PyroWaveVulkan;
namespace {
struct WindowIdentity { HWND hwnd; Uint32 id; };
WindowIdentity identity(SDL_Window* window) {
    SDL_SysWMinfo wm{}; SDL_VERSION(&wm.version);
    if (!SDL_GetWindowWMInfo(window, &wm) || wm.subsystem != SDL_SYSWM_WINDOWS)
        throw std::runtime_error("SDL Win32 identity unavailable");
    return {wm.info.win.window, SDL_GetWindowID(window)};
}
void verifyWindow(SDL_Window* window, WindowIdentity initial) {
    auto current = identity(window);
    if (current.hwnd != initial.hwnd || current.id != initial.id) throw std::runtime_error("HWND or SDL window ID changed");
}
bool sdlFallback(SDL_Window* window, WindowIdentity initial, const Log& log, bool hidden) {
    verifyWindow(window, initial);
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "direct3d11");
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) { log(std::string("same_window_d3d11=FAIL ") + SDL_GetError()); return false; }
    SDL_RendererInfo info{};
    SDL_GetRendererInfo(renderer, &info);
    const bool d3d = info.name && std::string(info.name) == "direct3d11";
    log(std::string("same_window_sdl_renderer=") + (info.name ? info.name : "unknown"));
    bool okay = d3d;
    const Uint64 until = SDL_GetTicks64() + (hidden ? 100 : 3000);
    do {
        okay = okay && SDL_SetRenderDrawColor(renderer, 25, 80, 25, 255) == 0 && SDL_RenderClear(renderer) == 0;
        int width = 0, height = 0;
        okay = okay && SDL_GetRendererOutputSize(renderer, &width, &height) == 0;
        SDL_Rect rectangle{width / 4, height / 4, width / 2, height / 2};
        okay = okay && SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255) == 0 && SDL_RenderFillRect(renderer, &rectangle) == 0;
        SDL_RenderPresent(renderer); // Existing SDL/D3D renderer, no interop.
        SDL_Event event{};
        SDL_WaitEventTimeout(&event, 16);
        verifyWindow(window, initial);
    } while (SDL_GetTicks64() < until);
    SDL_DestroyRenderer(renderer);
    verifyWindow(window, initial);
    log(std::string("same_window_d3d11=") + (okay ? "API_PASS" : "FAIL"));
    return okay;
}
}
int main(int argc, char** argv) {
    bool hidden = false, exercise = false, unflagged = false;
    int seconds = 15;
    std::string logPath = "vulkan-probe.log", missing;
    ProbeOptions options;
    try {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--hidden") hidden = true;
            else if (arg == "--exercise") exercise = true;
            else if (arg == "--without-vulkan-flag") unflagged = true;
            else if (arg == "--no-vsync") options.vsync = false;
            else if (arg == "--no-validation") options.validation = false;
            else if (arg == "--api-1-0") options.api10 = true;
            else if (arg == "--seconds" && i + 1 < argc) seconds = std::stoi(argv[++i]);
            else if (arg == "--log" && i + 1 < argc) logPath = argv[++i];
            else if (arg == "--expect-device" && i + 1 < argc) options.deviceName = argv[++i];
            else if (arg == "--fail-at" && i + 1 < argc) options.failAt = argv[++i];
            else if (arg == "--missing-export" && i + 1 < argc) missing = argv[++i];
            else throw std::runtime_error("unknown/incomplete argument " + arg);
        }
        if (seconds < 1 || seconds > 600) throw std::runtime_error("--seconds must be 1..600");
        const std::vector<std::string> failures{"instance","surface","device","command-pool","command-resources","swapchain","render-pass","image-view","framebuffer","present-semaphore"};
        if (!options.failAt.empty() && std::find(failures.begin(), failures.end(), options.failAt) == failures.end())
            throw std::runtime_error("unknown --fail-at checkpoint");
        if (!missing.empty() && missing != "vkCreateDevice" && missing != "vkCreateSwapchainKHR")
            throw std::runtime_error("--missing-export supports vkCreateDevice or vkCreateSwapchainKHR");
    } catch (const std::exception& e) { std::cerr << e.what() << "\n"; return 2; }
    std::ofstream evidence(logPath, std::ios::trunc);
    if (!evidence) { std::cerr << "Cannot write evidence log " << logPath << "\n"; return 2; }
    std::mutex logMutex;
    Log log = [&](const std::string& message) {
        std::lock_guard<std::mutex> guard(logMutex);
        std::cout << message << std::endl;
        evidence << message << std::endl;
    };
#if defined(_M_ARM64) || defined(__aarch64__)
    log("executable_arch=ARM64");
#elif defined(_M_X64) || defined(__x86_64__)
    log("executable_arch=x64");
#else
#error Stage 2 probe supports Windows ARM64 and x64 only
#endif
    log("scope=SYNTHETIC_ONLY production_streaming=UNCHANGED visual_qualification=OWNER_REQUIRED");
    Dispatch dispatch;
    try { dispatch.load(); } catch (const std::exception& e) { log(std::string("probe=SKIP ") + e.what()); return 77; }
    log("loader_path=" + dispatch.loaderPath);
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) { log(std::string("probe=SKIP SDL video unavailable: ") + SDL_GetError()); return 77; }
    if (SDL_Vulkan_LoadLibrary(dispatch.loaderPath.c_str())) {
        log(std::string("probe=SKIP SDL Vulkan loader unavailable: ") + SDL_GetError()); SDL_Quit(); return 77;
    }
    Uint32 flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    if (!unflagged) flags |= SDL_WINDOW_VULKAN;
    if (hidden) flags |= SDL_WINDOW_HIDDEN;
    SDL_Window* window = SDL_CreateWindow("Asteria Stage 2: Vulkan color bars; R recreate, F fullscreen, Esc -> SDL test",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 960, 540, flags);
    if (!window) {
        log(std::string("probe=SKIP SDL window unavailable: ") + SDL_GetError()); SDL_Vulkan_UnloadLibrary(); SDL_Quit(); return 77;
    }
    int result = 1;
    try {
        const auto initial = identity(window);
        log("sdl_window_id=" + std::to_string(initial.id) + " hwnd=" + std::to_string(reinterpret_cast<uintptr_t>(initial.hwnd)) + " creation_vulkan_flag=" + std::to_string(!unflagged));
        SDL_version runtime{}; SDL_GetVersion(&runtime);
        log("sdl_runtime=" + std::to_string(runtime.major) + "." + std::to_string(runtime.minor) + "." + std::to_string(runtime.patch));
        Probe probe(window, dispatch, log, options);
        dispatch.missingExport = missing;
        bool nativeOkay = false, expectedFailure = false, unavailable = false;
        try {
            probe.initialize();
            verifyWindow(window, initial);
            log("surface_window_identity=STABLE");
            const Uint64 started = SDL_GetTicks64();
            Uint64 nextDraw = started;
            int lastExercise = -1;
            bool running = true;
            while (running && SDL_GetTicks64() - started < static_cast<Uint64>(seconds) * 1000) {
                const Uint64 now = SDL_GetTicks64();
                if (exercise) {
                    const int step = static_cast<int>((now - started) / 1000);
                    if (step != lastExercise) {
                        lastExercise = step;
                        if (step == 2 && !hidden) { SDL_MinimizeWindow(window); log("exercise=minimize requested"); }
                        else if (step == 3 && !hidden) { SDL_RestoreWindow(window); log("exercise=restore requested"); }
                        else { SDL_SetWindowSize(window, step % 2 ? 640 : 960, step % 2 ? 480 : 540); probe.resize(); log("exercise=resize/recreate requested"); }
                    }
                }
                if (now >= nextDraw) { verifyWindow(window, initial); probe.draw(); nextDraw = SDL_GetTicks64() + Progress::retryMilliseconds; }
                SDL_Event event{};
                const Uint64 current = SDL_GetTicks64();
                const int wait = nextDraw > current ? static_cast<int>(nextDraw - current) : 0;
                if (SDL_WaitEventTimeout(&event, wait)) {
                    // Handle one event; the bounded timer, not event count, drives GPU queries.
                    if (event.type == SDL_QUIT) running = false;
                    if (event.type == SDL_KEYDOWN) {
                        if (event.key.keysym.sym == SDLK_ESCAPE) running = false;
                        if (event.key.keysym.sym == SDLK_r) probe.resize();
                        if (event.key.keysym.sym == SDLK_f) {
                            SDL_SetWindowFullscreen(window, SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
                            probe.resize();
                        }
                    }
                    if (event.type == SDL_WINDOWEVENT) {
                        log("window_event=" + std::to_string(event.window.event));
                        if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || event.window.event == SDL_WINDOWEVENT_RESTORED ||
                            event.window.event == SDL_WINDOWEVENT_DISPLAY_CHANGED) probe.resize();
                        // Expose needs only the next scheduled redraw, not a device drain/rebuild.
                        if (event.window.event == SDL_WINDOWEVENT_CLOSE) running = false;
                    }
                }
            }
            nativeOkay = probe.presents > 0;
            log("native_present_calls=" + std::to_string(probe.presents) + " swapchain_generations=" + std::to_string(probe.recreations));
            if (exercise && hidden) log("minimize_restore=SKIP hidden window; owner visible test required");
        } catch (const std::exception& e) {
            log(std::string("native_error=") + e.what());
            expectedFailure = probe.injectedFailure || (!missing.empty() && std::string(e.what()).find("missing Vulkan export: " + missing) != std::string::npos);
            unavailable = std::string(e.what()).find("no non-software physical device") != std::string::npos;
        }
        probe.close();
        verifyWindow(window, initial);
        log("native_teardown=DONE window_identity=STABLE");
        SDL_RestoreWindow(window);
        SDL_SetWindowFullscreen(window, 0);
        SDL_SetWindowTitle(window, "Asteria Stage 2: SAME window SDL/D3D11 green + white rectangle");
        const bool fallbackOkay = sdlFallback(window, initial, log, hidden);
        const auto errors = probe.validationErrors.load();
        log("validation_errors=" + std::to_string(errors) + " validation_status=" + (probe.validationActive ? (errors ? "FAIL" : "NO_REPORTED_ERRORS") : "SKIP"));
        if (expectedFailure) log("failure_injection=OBSERVED_AND_CLEANED");
        if (unavailable && fallbackOkay && !errors && probe.cleanupOkay) result = 77;
        else result = (nativeOkay || expectedFailure) && fallbackOkay && !errors && probe.cleanupOkay ? 0 : 1;
    } catch (const std::exception& e) { log(std::string("probe=FAIL ") + e.what()); }
    SDL_DestroyWindow(window);
    SDL_Vulkan_UnloadLibrary();
    SDL_Quit();
    log(std::string("probe=") + (result == 0 ? "API_PASS" : (result == 77 ? "SKIP" : "FAIL")) + " visual_qualification=NOT_AUTOMATED Stage3=NOT_AUTHORIZED");
    return result;
}
