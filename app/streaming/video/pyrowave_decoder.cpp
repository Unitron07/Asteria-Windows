#include "pyrowave_decoder.h"
#include "pyrowave_transport.h"
#include "streaming/session.h"
#include "streaming/streamutils.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <algorithm>
#include <new>

namespace {
// The C API does not expose the codec commit. Bind the experimental build's
// metadata to its DLL bytes before loading; do not pretend API proves bitstream.
bool verifyProvenance(const QString& directory, QString& reason) {
    QFile metadata(directory + "/pyrowave-runtime.json");
    if (!metadata.open(QIODevice::ReadOnly) || metadata.size() > 4096) {
        reason = "missing or oversized pyrowave-runtime.json provenance";
        return false;
    }
    const auto object = QJsonDocument::fromJson(metadata.readAll()).object();
#ifdef Q_PROCESSOR_ARM_64
    const QString architecture = "arm64";
#else
    const QString architecture = "x64";
#endif
    if (object["codecCommit"].toString() != PyroWave::CodecCommit ||
        object["bitstreamId"].toString() != PyroWave::BitstreamId ||
        object["apiVersion"].toString() != "0.6.0" ||
        object["architecture"].toString() != architecture) {
        reason = "runtime provenance must match codec 186f0393, API 0.6.0 and target " + architecture;
        return false;
    }
    QFile dll(directory + "/libpyrowave-shared-0.dll");
    if (!dll.open(QIODevice::ReadOnly)) { reason = "runtime DLL absent or inaccessible"; return false; }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    const auto expected = object["sha256"].toString().toLatin1().toLower();
    if (expected.size() != 64 || !hash.addData(&dll) || hash.result().toHex() != expected) {
        reason = "runtime DLL SHA-256 does not match its build provenance";
        return false;
    }
    return true;
}
}

PyroWaveVideoDecoder::~PyroWaveVideoDecoder() {
    // Session holds its decoder lock while destroying us, then stops common-c.
    if (!m_TestOnly) Session::get()->getOverlayManager().setOverlayRenderer(nullptr);
    for (auto texture : m_OverlayTextures) if (texture) SDL_DestroyTexture(texture);
    if (m_Texture) SDL_DestroyTexture(m_Texture);
    if (m_Renderer) SDL_DestroyRenderer(m_Renderer);
    m_Runtime.close();
}

QString PyroWaveVideoDecoder::getError() {
    std::lock_guard<std::mutex> guard(m_Mutex);
    return m_Error;
}

bool PyroWaveVideoDecoder::fail(const QString& reason) {
    std::lock_guard<std::mutex> guard(m_Mutex);
    if (!m_Failed) {
        m_Error = reason;
        m_Failed = true;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"PyroWave: %s",qPrintable(reason));
        if (!m_TestOnly) wakeRenderer();
    }
    return false;
}

bool PyroWaveVideoDecoder::initialize(PDECODER_PARAMETERS params) {
    // Even test-only preflight on Session's hidden main-thread window must
    // actually create and upload to the SDL renderer, before any host launch.
    if (params->videoFormat != VIDEO_FORMAT_PYROWAVE || !params->window ||
        !PyroWave::validLiveExtent(params->width,params->height))
        return fail("unsupported PyroWave profile or dimensions");
    if (params->vds == StreamingPreferences::VDS_FORCE_SOFTWARE)
        return fail("PyroWave requires Vulkan GPU decode; disable force software decoding");
    m_Width = params->width; m_Height = params->height;
    // Set only after successful initialization so a failed live probe does not
    // detach another decoder's overlay renderer or enqueue an orphan wakeup.
    const QString directory = QCoreApplication::applicationDirPath() + "/pyrowave";
    QString reason;
    if (!verifyProvenance(directory,reason)) return fail(reason);
    if (!m_Runtime.load(std::filesystem::path(directory.toStdWString())) ||
        !m_Runtime.createDecoder(m_Width,m_Height))
        return fail(QString::fromStdString(m_Runtime.error()));
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "PyroWave preflight: runtime/API=0.6.0 bitstream=%s extent=%dx%d GPU=%s",
                PyroWave::BitstreamId,m_Width,m_Height,m_Runtime.deviceDescription().c_str());

    Uint32 flags = SDL_RENDERER_ACCELERATED;
    if (params->enableVsync) flags |= SDL_RENDERER_PRESENTVSYNC;
    m_Renderer = SDL_CreateRenderer(params->window,-1,flags);
    if (!m_Renderer) return fail(QString("SDL renderer creation: ") + SDL_GetError());
    // Qualified v15 SDL2-compat captures BT709_LIMITED at IYUV creation.
    SDL_SetYUVConversionMode(SDL_YUV_CONVERSION_BT709);
    m_Texture = SDL_CreateTexture(m_Renderer,SDL_PIXELFORMAT_IYUV,
                                  SDL_TEXTUREACCESS_STREAMING,m_Width,m_Height);
    if (!m_Texture || SDL_SetTextureBlendMode(m_Texture,SDL_BLENDMODE_NONE) != 0 ||
        SDL_SetTextureScaleMode(m_Texture,SDL_ScaleModeLinear) != 0)
        return fail(QString("SDL IYUV creation: ") + SDL_GetError());
    try {
        PyroWave::Pixels black;
        for (int p=0;p<3;++p) black.planes[p].assign(std::size_t(m_Width >> (p!=0)) *
            (m_Height >> (p!=0)),p ? 128 : 16);
        if (SDL_UpdateYUVTexture(m_Texture,nullptr,black.planes[0].data(),m_Width,
            black.planes[1].data(),m_Width/2,black.planes[2].data(),m_Width/2) != 0 ||
            SDL_SetRenderDrawColor(m_Renderer,0,0,0,255) != 0 ||
            SDL_RenderClear(m_Renderer) != 0 || SDL_RenderCopy(m_Renderer,m_Texture,nullptr,nullptr) != 0)
            return fail(QString("SDL IYUV preflight upload/presentation: ") + SDL_GetError());
        SDL_RenderPresent(m_Renderer);
    } catch (const std::bad_alloc&) { return fail("I420 preflight allocation failed"); }
    SDL_RendererInfo info{};
    SDL_GetRendererInfo(m_Renderer,&info);
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,"PyroWave SDL presentation initialized: %s I420 BT.709 limited",
                info.name ? info.name : "unknown");
    m_TestOnly = params->testOnly;
    if (!m_TestOnly) Session::get()->getOverlayManager().setOverlayRenderer(this);
    return true;
}

void PyroWaveVideoDecoder::wakeRenderer() {
    if (m_EventQueued) return;
    SDL_Event event{};
    event.type = SDL_USEREVENT; event.user.code = SDL_CODE_FRAME_READY;
    m_EventQueued = SDL_PushEvent(&event) == 1;
}

int PyroWaveVideoDecoder::submitDecodeUnit(PDECODE_UNIT du) {
    {
        std::lock_guard<std::mutex> guard(m_Mutex);
        if (m_Failed) { wakeRenderer(); return DR_OK; }
        const uint64_t now = LiGetMicroseconds();
        if (!m_Stats.measurementStartUs) m_Stats.measurementStartUs = now;
        if (m_LastFrameNumber && du->frameNumber > m_LastFrameNumber + 1) {
            const auto lost = du->frameNumber - m_LastFrameNumber - 1;
            m_Stats.networkDroppedFrames += lost; m_Stats.totalFrames += lost;
        }
        m_LastFrameNumber = du->frameNumber;
        ++m_Stats.receivedFrames; ++m_Stats.totalFrames;
        m_Stats.totalReassemblyTimeUs += du->enqueueTimeUs - du->receiveTimeUs;
        if (du->fullLength > 0) m_Bytes += du->fullLength;
        if (du->frameHostProcessingLatency) {
            const auto latency = du->frameHostProcessingLatency;
            m_Stats.minHostProcessingLatency = m_Stats.minHostProcessingLatency ?
                std::min(m_Stats.minHostProcessingLatency,latency) : latency;
            m_Stats.maxHostProcessingLatency = std::max(m_Stats.maxHostProcessingLatency,latency);
            m_Stats.totalHostProcessingLatency += latency;
            ++m_Stats.framesWithHostProcessingLatency;
        }
    }
    PyroWave::Pixels pixels;
    std::size_t packets = 0;
    std::string rejection;
    try {
        std::vector<std::uint8_t> bytes;
        if (PyroWave::assembleLiveDecodeUnit(*du,bytes,rejection) &&
            !m_Runtime.decodeLive(bytes,pixels,packets)) {
            if (m_Runtime.frameRejected()) rejection = m_Runtime.error();
            else { fail(QString::fromStdString(m_Runtime.error())); return DR_OK; }
        }
    } catch (const std::bad_alloc&) { fail("live frame allocation failed"); return DR_OK; }
    if (!rejection.empty()) {
        // Includes reassembly rejection before any runtime packet submission.
        m_Runtime.discardFrame();
        if (++m_Rejected <= 5 || (m_Rejected % 256) == 0)
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,"PyroWave rejected frame %u: %s (rejected=%llu)",
                        du->frameNumber,rejection.c_str(),static_cast<unsigned long long>(m_Rejected));
        return DR_OK; // Independent frames: no IDR/hot-switch or partial recovery
    }
    std::lock_guard<std::mutex> guard(m_Mutex);
    if (m_FirstFrame) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "First valid PyroWave decode unit: %d bytes, compatibility packets=%zu; first I420 decode succeeded",
                    du->fullLength,packets);
        m_FirstFrame = false;
    }
    ++m_Stats.decodedFrames;
    m_Stats.totalDecodeTimeUs += LiGetMicroseconds() - du->enqueueTimeUs;
    if (!m_Pending.planes[0].empty()) ++m_Stats.pacerDroppedFrames;
    m_Pending = std::move(pixels);
    wakeRenderer();
    return DR_OK;
}

void PyroWaveVideoDecoder::updateStats() {
    VIDEO_STATS stats;
    uint64_t bytes;
    const auto now = LiGetMicroseconds();
    if (now - m_StatsTime < 1000000) return;
    m_StatsTime = now;
    {
        std::lock_guard<std::mutex> guard(m_Mutex);
        stats = m_Stats; bytes = m_Bytes;
    }
    if (!stats.measurementStartUs) return;
    const double seconds = double(now - stats.measurementStartUs) / 1000000.0;
    if (seconds <= 0) return;
    LiGetEstimatedRttInfo(&stats.lastRtt,&stats.lastRttVariance);
    char text[768];
    snprintf(text,sizeof(text),
        "PyroWave SDR 8-bit 4:2:0 %dx%d\nIncoming / decoded / rendered: %.1f / %.1f / %.1f FPS\n"
        "Video: %.1f Mbps; network dropped: %u; presentation dropped: %u\n"
        "Average decode: %.2f ms; render: %.2f ms; RTT: %u ms (variance %u ms)",
        m_Width,m_Height,stats.receivedFrames/seconds,stats.decodedFrames/seconds,
        stats.renderedFrames/seconds,bytes*8.0/seconds/1000000.0,
        stats.networkDroppedFrames,stats.pacerDroppedFrames,
        stats.decodedFrames ? stats.totalDecodeTimeUs/1000.0/stats.decodedFrames : 0,
        stats.renderedFrames ? stats.totalRenderTimeUs/1000.0/stats.renderedFrames : 0,
        stats.lastRtt,stats.lastRttVariance);
    auto& overlays = Session::get()->getOverlayManager();
    if (overlays.isOverlayEnabled(Overlay::OverlayDebug)) overlays.updateOverlayText(Overlay::OverlayDebug,text);
}

void PyroWaveVideoDecoder::renderOverlays() {
    auto& manager = Session::get()->getOverlayManager();
    int width=0,height=0;
    SDL_GetRendererOutputSize(m_Renderer,&width,&height);
    for (int i=0;i<Overlay::OverlayMax;++i) {
        const auto type = static_cast<Overlay::OverlayType>(i);
        if (!manager.isOverlayEnabled(type)) continue;
        if (auto surface = manager.getUpdatedOverlaySurface(type)) {
            if (m_OverlayTextures[i]) SDL_DestroyTexture(m_OverlayTextures[i]);
            m_OverlayTextures[i] = SDL_CreateTextureFromSurface(m_Renderer,surface);
            SDL_FreeSurface(surface);
        }
        if (m_OverlayTextures[i]) {
            SDL_Rect rect{0,0,0,0};
            SDL_QueryTexture(m_OverlayTextures[i],nullptr,nullptr,&rect.w,&rect.h);
            if (type == Overlay::OverlayStatusUpdate) rect.y = height - rect.h;
            SDL_RenderCopy(m_Renderer,m_OverlayTextures[i],nullptr,&rect);
        }
    }
}

void PyroWaveVideoDecoder::renderFrameOnMainThread() {
    PyroWave::Pixels pixels;
    {
        std::lock_guard<std::mutex> guard(m_Mutex);
        m_EventQueued = false;
        if (m_Failed) return; // Session reads getError() and tears down normally
        pixels = std::move(m_Pending);
        m_Pending = {};
    }
    const auto start = LiGetMicroseconds();
    if (!pixels.planes[0].empty()) {
        if (SDL_UpdateYUVTexture(m_Texture,nullptr,pixels.planes[0].data(),m_Width,
            pixels.planes[1].data(),m_Width/2,pixels.planes[2].data(),m_Width/2) != 0) {
            fail(QString("SDL I420 upload: ") + SDL_GetError()); return;
        }
        m_HaveTextureFrame = true;
    }
    updateStats();
    if (!m_HaveTextureFrame) return;
    SDL_Rect source{0,0,m_Width,m_Height}, destination{0,0,0,0};
    if (SDL_GetRendererOutputSize(m_Renderer,&destination.w,&destination.h) != 0) {
        fail(QString("SDL output size: ") + SDL_GetError()); return;
    }
    if (!destination.w || !destination.h) return;
    StreamUtils::scaleSourceToDestinationSurface(&source,&destination);
    if (SDL_RenderClear(m_Renderer) != 0 ||
        SDL_RenderCopy(m_Renderer,m_Texture,nullptr,&destination) != 0) {
        fail(QString("SDL presentation: ") + SDL_GetError()); return;
    }
    renderOverlays();
    SDL_RenderPresent(m_Renderer);
    if (!pixels.planes[0].empty()) {
        std::lock_guard<std::mutex> guard(m_Mutex);
        ++m_Stats.renderedFrames;
        m_Stats.totalRenderTimeUs += LiGetMicroseconds() - start;
    }
}

void PyroWaveVideoDecoder::notifyOverlayUpdated(Overlay::OverlayType) {
    std::lock_guard<std::mutex> guard(m_Mutex);
    wakeRenderer();
}

bool PyroWaveVideoDecoder::notifyWindowChanged(PWINDOW_STATE_CHANGE_INFO info) {
    // Output size and aspect fit are recalculated on the main thread each render.
    return !(info->stateChangeFlags & ~(WINDOW_STATE_CHANGE_SIZE | WINDOW_STATE_CHANGE_DISPLAY));
}
