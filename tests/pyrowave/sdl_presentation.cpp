#define SDL_MAIN_HANDLED
#include <SDL.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include "pyrowave_runtime.h"
#include "presentation_patterns.h"
#include "sdl_presentation.h"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <thread>

namespace {
using namespace Presentation;
using Clock=std::chrono::steady_clock;
struct Unavailable : std::runtime_error { using std::runtime_error::runtime_error; };
struct Log {
    std::ofstream file;
    explicit Log(const std::filesystem::path& dir) {
        if (!dir.empty()) {
            std::filesystem::create_directories(dir);
            file.open(dir/"presentation.txt");
            if (!file) throw std::runtime_error("cannot write presentation evidence");
        }
    }
    void line(const std::string& s) {
        std::cout<<s<<std::endl;
        if (file.is_open()) { file<<s<<std::endl; if (!file) throw std::runtime_error("evidence write failed"); }
    }
};
void require(bool ok,const char* operation) {
    if (!ok) throw std::runtime_error(std::string(operation)+": "+SDL_GetError());
}
std::string hash(const PyroWave::Pixels& p) {
    struct Hasher {
        BCRYPT_ALG_HANDLE algorithm=nullptr;
        BCRYPT_HASH_HANDLE digest=nullptr;
        ~Hasher() { if (digest) BCryptDestroyHash(digest); if (algorithm) BCryptCloseAlgorithmProvider(algorithm,0); }
    } h;
    auto check=[](NTSTATUS status) { if (status<0) throw std::runtime_error("SHA256 BCrypt failure"); };
    check(BCryptOpenAlgorithmProvider(&h.algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0));
    check(BCryptCreateHash(h.algorithm,&h.digest,nullptr,0,nullptr,0,0));
    for (const auto& plane:p.planes)
        check(BCryptHashData(h.digest,const_cast<PUCHAR>(plane.data()),ULONG(plane.size()),0));
    unsigned char bytes[32]; check(BCryptFinishHash(h.digest,bytes,32,0));
    std::ostringstream out; out<<std::hex<<std::setfill('0');
    for (auto b:bytes) out<<std::setw(2)<<unsigned(b);
    return out.str();
}
struct Resources {
    SDL_Window* window=nullptr;
    SDL_Renderer* renderer=nullptr;
    SDL_Texture* texture=nullptr;
    bool initialized=false, linear=false;
    ~Resources() { shutdown(); }
    void destroyRenderer() {
        if (texture) SDL_DestroyTexture(texture);
        texture=nullptr;
        if (renderer) SDL_DestroyRenderer(renderer);
        renderer=nullptr;
    }
    void shutdown() {
        destroyRenderer();
        if (window) SDL_DestroyWindow(window);
        window=nullptr;
        if (initialized) SDL_Quit();
        initialized=false;
    }
    void makeTexture() {
        if (texture) SDL_DestroyTexture(texture);
        texture=nullptr;
        // In pinned sdl2-compat this is captured as BT709_LIMITED at texture creation.
        SDL_SetYUVConversionMode(SDL_YUV_CONVERSION_BT709);
        texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_IYUV,SDL_TEXTUREACCESS_STREAMING,1920,1080);
        require(texture!=nullptr,"SDL_CreateTexture IYUV");
        require(SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_NONE)==0,"texture blend mode");
        require(SDL_SetTextureScaleMode(texture,linear ? SDL_ScaleModeLinear : SDL_ScaleModeNearest)==0,"texture scale mode");
    }
    void makeRenderer(bool software) {
        renderer=SDL_CreateRenderer(window,-1,software ? SDL_RENDERER_SOFTWARE : SDL_RENDERER_ACCELERATED);
        if (!renderer) throw Unavailable(std::string("SDL renderer unavailable: ")+SDL_GetError());
        makeTexture();
    }
    void open(bool software,bool hidden) {
        SDL_SetMainReady();
        if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)!=0)
            throw Unavailable(std::string("SDL video unavailable: ")+SDL_GetError());
        initialized=true;
        window=SDL_CreateWindow("Asteria offline P0.5 - 1..5 patterns, T source, F fullscreen, R recreate, Esc exit",
            SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1920,1080,
            SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI|(hidden ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN));
        if (!window) throw Unavailable(std::string("SDL window unavailable: ")+SDL_GetError());
        makeRenderer(software);
    }
    void upload(const PyroWave::Pixels& p) {
        if (!PyroWave::validProofPixels(p)) throw std::runtime_error("invalid I420 extents/plane sizes");
        require(SDL_UpdateYUVTexture(texture,nullptr,p.planes[0].data(),1920,
            p.planes[1].data(),960,p.planes[2].data(),960)==0,"SDL_UpdateYUVTexture Y/U/V pitches 1920/960/960");
    }
    void present() {
        int w=0,h=0;
        require(SDL_GetRendererOutputSize(renderer,&w,&h)==0,"renderer output size");
        const auto rect=fit(w,h); // Output pixels, not DPI-scaled window coordinates.
        if (!rect.w || !rect.h) return; // Minimized window.
        const SDL_Rect dst{rect.x,rect.y,rect.w,rect.h};
        require(SDL_SetRenderDrawColor(renderer,0,0,0,255)==0,"render clear color");
        require(SDL_RenderClear(renderer)==0,"SDL_RenderClear");
        require(SDL_RenderCopy(renderer,texture,nullptr,&dst)==0,"SDL_RenderCopy");
        SDL_RenderPresent(renderer); // SDL2 has no return value; submission != visually qualified.
    }
    void report(Log& log) {
        SDL_RendererInfo info{}; require(SDL_GetRendererInfo(renderer,&info)==0,"renderer info");
        bool iyuv=false;
        for (Uint32 i=0;i<info.num_texture_formats;++i) iyuv|=info.texture_formats[i]==SDL_PIXELFORMAT_IYUV;
        int ww=0,wh=0,ow=0,oh=0;
        SDL_GetWindowSize(window,&ww,&wh);
        require(SDL_GetRendererOutputSize(renderer,&ow,&oh)==0,"output dimensions");
        Uint32 format=0; int access=0,tw=0,th=0;
        require(SDL_QueryTexture(texture,&format,&access,&tw,&th)==0,"texture query");
        log.line("renderer="+std::string(info.name ? info.name : "unknown")+" flags="+std::to_string(info.flags)+
            " advertisedIYUV="+(iyuv ? "yes" : "no (SDL may convert/emulate)")+" actualTextureFormat="+SDL_GetPixelFormatName(format)+
            " texture="+std::to_string(tw)+"x"+std::to_string(th)+" access="+std::to_string(access));
        log.line("window="+std::to_string(ww)+"x"+std::to_string(wh)+" output="+std::to_string(ow)+"x"+std::to_string(oh)+
            " source=1920x1080 pitches=1920,960,960 scaling="+(linear ? "linear" : "nearest")+" aspect=fit");
    }
};
const char* sourceName(int source) { return source==0 ? "raw-i420" : source==1 ? "compatibility" : "records"; }
struct Input {
    PyroWave::Runtime runtime;
    PyroWave::Pixels pixels[3];
    std::filesystem::path directory, evidence;
    bool loaded=false;
    void prepare(Pattern kind,int source,Log& log) {
        pixels[0]=pattern(kind);
        if (source && !loaded) {
            if (!runtime.load(directory)) throw std::runtime_error(runtime.error());
            if (!runtime.createDecoder(1920,1080)) {
                if (runtime.error().find("native system Vulkan loader unavailable")!=std::string::npos ||
                    runtime.error().find("device creation failed: -5")!=std::string::npos)
                    throw Unavailable(runtime.error());
                throw std::runtime_error(runtime.error());
            }
            loaded=true;
            log.line("runtimeDirectory="+directory.string()+" API=0.6.0 requiredExports=loaded");
            log.line("Vulkan adapter="+runtime.deviceDescription());
        }
        if (source) {
            std::vector<std::uint8_t> compatibility, records;
            if (!runtime.encodeProofPixels(pixels[0],compatibility)) throw std::runtime_error(runtime.error());
            PyroWave::Frame frame; std::string error;
            if (!PyroWave::parseCompatibilityFrame(compatibility.data(),compatibility.size(),compatibility.size(),frame,error))
                throw std::runtime_error(error);
            // Same encoded packet stream, serialized two ways. No second encoder variance.
            for (const auto& packet:frame.packets)
                records.insert(records.end(),compatibility.begin()+packet.offset,compatibility.begin()+packet.offset+packet.size);
            if (!runtime.decode(compatibility,pixels[1]) || !runtime.decode(records,pixels[2]))
                throw std::runtime_error(runtime.error());
            for (int p=0;p<3;++p) if (pixels[1].planes[p]!=pixels[2].planes[p])
                throw std::runtime_error("framing modes decoded different pixels");
            std::uint64_t delta=0; int maximum=0;
            for (int p=0;p<3;++p) for (std::size_t i=0;i<pixels[0].planes[p].size();++i) {
                const int d=std::abs(int(pixels[0].planes[p][i])-int(pixels[1].planes[p][i]));
                delta+=d; maximum=std::max(maximum,d);
            }
            log.line("PASS framing equality pattern="+std::string(name(kind))+" frameBytes="+std::to_string(compatibility.size())+
                ","+std::to_string(records.size())+" sampleMAE="+std::to_string(double(delta)/3110400)+" maxError="+std::to_string(maximum));
            // Sharp-edge patterns are intentionally lossy; report error without gray-ramp's <=8 gate.
        }
        log.line("selectedPattern="+std::string(name(kind))+" framing="+sourceName(source)+" I420-SHA256="+hash(pixels[source]));
        if (!evidence.empty()) {
            std::ofstream out(evidence/(std::string(name(kind))+"-"+sourceName(source)+".i420"),std::ios::binary);
            for (const auto& plane:pixels[source].planes) out.write(reinterpret_cast<const char*>(plane.data()),std::streamsize(plane.size()));
            if (!out) throw std::runtime_error("I420 evidence write failed");
        }
    }
    void reload() { runtime.close(); loaded=false; }
};
void reset(Resources& r,Input& input,Pattern pattern,int source,Reset reason,bool software,Log& log) {
    log.line(std::string("resourceRecovery=")+(reason==Reset::Device ? "SDL_RENDER_DEVICE_RESET" :
        reason==Reset::Targets ? "SDL_RENDER_TARGETS_RESET" : "synthetic full renderer + decoder + runtime recreation"));
    if (recreateRenderer(reason)) {
        r.destroyRenderer(); input.reload(); input.prepare(pattern,source,log); r.makeRenderer(software);
    } else r.makeTexture();
    r.upload(input.pixels[source]); r.present(); r.report(log);
}
}

int runPresentation(int argc,char** argv) {
    std::unique_ptr<Log> log;
    bool submitted=false;
    try {
        const std::string mode=argv[1];
        if (argc<3) throw std::invalid_argument("Usage: --present-raw-i420|--present-compatibility|--present-records|--present-recreate-test|--present-loop|--present-smoke DLL_DIRECTORY_OR_DASH [EVIDENCE_DIRECTORY] [--pattern 1..5] [--seconds 1..3600] [--software] [--hidden]");
        int source=mode=="--present-raw-i420" || mode=="--present-smoke" ? 0 : mode=="--present-records" ? 2 : 1;
        const bool cycles=mode=="--present-recreate-test" || mode=="--present-smoke";
        if (mode!="--present-raw-i420" && mode!="--present-compatibility" && mode!="--present-records" &&
            mode!="--present-recreate-test" && mode!="--present-loop" && mode!="--present-smoke") return 2;
        int seconds=mode=="--present-loop" ? 10 : 0;
        Pattern kind=Pattern::Range; bool software=false,hidden=false;
        std::filesystem::path evidence;
        int i=3;
        if (i<argc && std::string(argv[i]).rfind("--",0)!=0) evidence=std::filesystem::absolute(argv[i++]);
        auto number=[](const char* text,int lo,int hi) {
            std::size_t count=0; const int value=std::stoi(text,&count);
            if (count!=std::string(text).size() || value<lo || value>hi) throw std::invalid_argument("numeric option out of range");
            return value;
        };
        for (;i<argc;++i) {
            const std::string option=argv[i];
            if (option=="--software") software=true;
            else if (option=="--hidden") hidden=true;
            else if (option=="--pattern" && i+1<argc) kind=Pattern(number(argv[++i],1,5)-1);
            else if (option=="--seconds" && i+1<argc) seconds=number(argv[++i],1,3600);
            else throw std::invalid_argument("unknown or incomplete presentation option");
        }
        if (hidden && !cycles && !seconds) throw std::invalid_argument("hidden inspection needs --seconds");
        if (source && std::string(argv[2])=="-") throw std::invalid_argument("codec modes require a runtime directory");
        log=std::make_unique<Log>(evidence);
        log->line("P0.5 offline SDR 8-bit I420 BT.709 limited range; codec="+std::string(PyroWave::CodecCommit)+" bitstream="+PyroWave::BitstreamId+" API=0.6.0");
        SDL_version compiled{},runtime{}; SDL_VERSION(&compiled); SDL_GetVersion(&runtime);
        auto version=[](const SDL_version& v) { return std::to_string(v.major)+"."+std::to_string(v.minor)+"."+std::to_string(v.patch); };
        log->line("SDL compiled="+version(compiled)+" runtime="+version(runtime)+" revision="+SDL_GetRevision());
        log->line("Chroma limit: authored CENTER; pinned sdl2-compat BT709_LIMITED uses SDL3 LEFT metadata. SDL2 has no independent siting control; backend alignment must be inspected and may fail this qualification.");
        log->line("Controls: 1 range, 2 BT709 bars, 3 centered chroma, 4 geometry, 5 gradient; T raw/compatibility/records; F fullscreen; R full recreation; S nearest/linear; N 1920x1080; W cycle 960x540/2560x1440/1000x1000; Esc exit.");
        log->line("Inspect: range 0/8/15/16 black and 235/236/247/255 white clip in limited conversion; 17/234 remain near endpoints. Bars white/yellow/cyan/green/magenta/red/blue/black. Chroma boundaries meet even luma grid; compare raw at native nearest. Center square stays square; corners visible, fit adds black bars. See README for manual expectations.");
        log->line("SDL_RenderPresent has no return value. Successful submissions do not prove visible color/chroma or device-loss recovery. No leak measurement is inferred from lifecycle success.");
        Input input; input.directory=std::filesystem::absolute(argv[2]); input.evidence=evidence;
        const auto start=Clock::now(); Timing timing;
        for (int cycle=0;cycle<(cycles ? 10 : 1);++cycle) {
            Resources resources; resources.open(software,hidden);
            input.reload();
            if (cycles) { kind=Pattern(cycle%5); if (mode=="--present-recreate-test") source=1+cycle%2; }
            input.prepare(kind,source,*log); resources.upload(input.pixels[source]); resources.report(*log);
            if (!source) log->line("Vulkan adapter=not requested (raw I420 mode); runtime=not loaded");
            resources.present(); submitted=true;
            if (cycles) {
                reset(resources,input,kind,source,Reset::Targets,software,*log);
                reset(resources,input,kind,source,Reset::Device,software,*log);
                reset(resources,input,kind,source,Reset::Resources,software,*log);
                SDL_PumpEvents(); SDL_Delay(100);
                log->line("PASS lifecycle cycle="+std::to_string(cycle)+" (synthetic reset/resource coverage, not hardware loss)");
                // resources destructor destroys texture/renderer/window and SDL_Quit each cycle.
                continue;
            }
            bool quit=false; int windowPreset=0;
            auto last=Clock::now(),deadline=last;
            const auto loopStart=last;
            while (!quit) {
                SDL_Event event{};
                while (SDL_PollEvent(&event)) {
                    if (event.type==SDL_QUIT) quit=true;
                    else if (event.type==SDL_RENDER_DEVICE_RESET || event.type==SDL_RENDER_TARGETS_RESET)
                        reset(resources,input,kind,source,event.type==SDL_RENDER_DEVICE_RESET ? Reset::Device : Reset::Targets,software,*log);
                    else if (event.type==SDL_WINDOWEVENT) {
                        if (event.window.event==SDL_WINDOWEVENT_CLOSE) quit=true;
                        if (event.window.event==SDL_WINDOWEVENT_SIZE_CHANGED || event.window.event==SDL_WINDOWEVENT_RESTORED ||
                            event.window.event==SDL_WINDOWEVENT_MAXIMIZED) resources.report(*log);
                    } else if (event.type==SDL_KEYDOWN && !event.key.repeat) {
                        const auto key=event.key.keysym.sym;
                        if (key==SDLK_ESCAPE) quit=true;
                        else if (key>=SDLK_1 && key<=SDLK_5) {
                            kind=Pattern(key-SDLK_1); input.prepare(kind,source,*log); resources.upload(input.pixels[source]);
                        } else if (key==SDLK_t) {
                            source=(source+1)%3; input.prepare(kind,source,*log); resources.upload(input.pixels[source]);
                        } else if (key==SDLK_r) reset(resources,input,kind,source,Reset::Resources,software,*log);
                        else if (key==SDLK_s) {
                            resources.linear=!resources.linear; resources.makeTexture(); resources.upload(input.pixels[source]); resources.report(*log);
                        } else if (key==SDLK_f) {
                            require(SDL_SetWindowFullscreen(resources.window,(SDL_GetWindowFlags(resources.window)&SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP)==0,"fullscreen toggle");
                            resources.report(*log);
                        } else if (key==SDLK_n) SDL_SetWindowSize(resources.window,1920,1080);
                        else if (key==SDLK_w) {
                            constexpr int sizes[3][2]={{960,540},{2560,1440},{1000,1000}};
                            SDL_SetWindowSize(resources.window,sizes[windowPreset][0],sizes[windowPreset][1]); windowPreset=(windowPreset+1)%3;
                        }
                    }
                }
                if (quit) break;
                deadline+=std::chrono::nanoseconds(16666667);
                std::this_thread::sleep_until(deadline);
                resources.present();
                const auto now=Clock::now(); timing.add(std::chrono::duration<double,std::milli>(now-last).count()); last=now;
                // Avoid a burst of catch-up submissions after an expensive recreate/resize/decode.
                if (now>deadline+std::chrono::milliseconds(17)) deadline=now;
                if (seconds && std::chrono::duration<double>(now-loopStart).count()>=seconds) quit=true;
            }
        }
        input.reload();
        log->line("timing requestedFPS=60 durationSeconds="+std::to_string(std::chrono::duration<double>(Clock::now()-start).count())+
            " presentLoopSeconds="+std::to_string(timing.sum/1000)+
            " intervals="+std::to_string(timing.intervals)+" averageMs="+std::to_string(timing.intervals ? timing.sum/timing.intervals : 0)+
            " minMs="+std::to_string(timing.minimum)+" maxMs="+std::to_string(timing.maximum)+" missedIntervals="+std::to_string(timing.missed)+
            " (submission intervals include interactive stalls; no latency benchmark)");
        log->line("PASS API uploads/render submissions and clean shutdown; visual qualification=PENDING owner; true device loss=UNPROVEN");
        return 0;
    } catch (const Unavailable& e) {
        const std::string result=(submitted ? "FAIL availability lost after submission: " : "SKIP unavailable presentation/device: ")+std::string(e.what());
        if (log) log->line(result); else std::cerr<<result<<'\n';
        return submitted ? 1 : 77;
    } catch (const std::exception& e) {
        if (log) log->line(std::string("FAIL: ")+e.what()); else std::cerr<<e.what()<<'\n';
        return 1;
    }
}
