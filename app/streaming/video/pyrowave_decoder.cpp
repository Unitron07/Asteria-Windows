#include "pyrowave_decoder.h"
#include "pyrowave_transport.h"
#include "pyrowave_sdl.h"
#include "streaming/session.h"
#include "streaming/streamutils.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include "pyrowave_perf_export.h"
#include <QSysInfo>
#include <algorithm>
#include <new>
#include <cstring>

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
        object["graniteCommit"].toString() != "b6cffd5ce81f540f0855e6778428483e14763d9b" ||
        object["factoryCleanupPatchSha256"].toString() != "8fe5906706bb27814ed7344f8932cd5ccf00c368ed24d253839d13cfeeb78c86" ||
        object["architecture"].toString() != architecture) {
        reason = "runtime provenance must match codec 186f0393, API 0.6.0, pinned Granite/factory cleanup and target " + architecture;
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
    const auto cleanupStarted=m_Perf ? PyroWavePerf::nowUs() : 0;
    if(m_Perf) m_Perf->end=cleanupStarted;
    // Session holds its decoder lock while destroying us, then stops common-c.
    if (!m_TestOnly) Session::get()->getOverlayManager().setOverlayRenderer(nullptr);
    if(m_RetryTimer) SDL_RemoveTimer(m_RetryTimer);
    const bool native=bool(m_Native);
    if(m_Native) { m_Native->shutdown(); logNativeSummary(); m_Native.reset(); }
    for (auto texture : m_OverlayTextures) if (texture) SDL_DestroyTexture(texture);
    m_Runtime.resetDecoder();
    // Decoder destruction drains its Vulkan work; report while the device lives.
    if (!native && !m_FirstFrame) reportGpuTiming();
    m_Gpu.reset();
    if (m_Texture) SDL_DestroyTexture(m_Texture);
    if (m_Renderer) SDL_DestroyRenderer(m_Renderer);
    m_Runtime.close();
    if(m_Perf) {
        m_Perf->metrics[PyroWavePerf::Cleanup].record(PyroWavePerf::nowUs()-cleanupStarted);
        exportPerf();
    }
}

QString PyroWaveVideoDecoder::getError() {
    std::lock_guard<std::mutex> guard(m_Mutex);
    return m_Error;
}

bool PyroWaveVideoDecoder::fail(const QString& reason) {
    std::lock_guard<std::mutex> guard(m_Mutex);
    if (!m_Failed) {
        m_Error = reason;
        m_Failed = true; m_BackendSelection.fatal();
        if(m_Native) m_Native->fatal(reason.toStdString());
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"PyroWave: %s",qPrintable(reason));
        if (!m_TestOnly) wakeRenderer();
    }
    return false;
}

bool PyroWaveVideoDecoder::initialize(PDECODER_PARAMETERS params) {
    configurePerf(params->testOnly);
    const auto initStarted=m_Perf ? PyroWavePerf::nowUs() : 0;
    m_TargetFps=params->frameRate; m_Vsync=params->enableVsync;
    if(m_Perf) m_BitrateKbps=Session::get()->performanceBitrateKbps();
    // Even test-only preflight on Session's hidden main-thread window must
    // prove presentation on the selected backend before any host launch.
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
    if(!m_Runtime.load(std::filesystem::path(directory.toStdWString()))) return fail(QString::fromStdString(m_Runtime.error()));
    try {
        m_BackendSelection.initialize([&] {
    m_Native=std::make_unique<PyroWave::NativePresentation>(m_Runtime,m_Perf.get());
    if(m_Native->initialize(params->window,m_Width,m_Height,params->enableVsync)) {
        return true;
    }
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,"PyroWave native initialization unavailable: %s; trying legacy D3D11/Vulkan interop",m_Native->error().c_str());
    // Initialization fallback only: native wrapper, WSI and device are gone before
    // the default device and historical SDL/D3D11 backend can be constructed.
    m_Native.reset();
            return false;
        },[&] { return initializeLegacy(params); },[&] { return initializeCpu(); });
    } catch(const std::exception& e) { return fail(QString::fromUtf8(e.what())); }
    m_TestOnly=params->testOnly;
    if(!m_TestOnly) {
        auto& overlays=Session::get()->getOverlayManager(); overlays.setOverlayRenderer(this);
        if(m_Native) for(int i=0;i<Overlay::OverlayMax;++i) {
            const auto type=static_cast<Overlay::OverlayType>(i);
            if(overlays.isOverlayEnabled(type)) overlays.setOverlayTextUpdated(type);
        }
    }
    if(m_Perf) {
        m_PerfGpu=QString::fromStdString(m_Native ? m_Native->diagnostics.gpu : m_Runtime.deviceDescription());
        m_PerfBackend=m_Native ? "NATIVE_VULKAN" : m_Gpu ? "LEGACY_D3D11_INTEROP" : "CPU_I420_FALLBACK";
        m_PerfDecoderPath=m_Runtime.decoderPath();
        m_Perf->metrics[PyroWavePerf::Initialization].record(PyroWavePerf::nowUs()-initStarted);
        m_Perf->start=PyroWavePerf::nowUs();
    }
    return true;
}

bool PyroWaveVideoDecoder::initializeLegacy(PDECODER_PARAMETERS params) {
    const QString directory=QCoreApplication::applicationDirPath()+"/pyrowave";
    if (!m_Runtime.load(std::filesystem::path(directory.toStdWString())) ||
        !m_Runtime.createDecoder(m_Width,m_Height,true))
        return fail(QString::fromStdString(m_Runtime.error()));
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "PyroWave preflight: runtime/API=0.6.0 bitstream=%s extent=%dx%d GPU=%s",
                PyroWave::BitstreamId,m_Width,m_Height,m_Runtime.deviceDescription().c_str());
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,"PyroWave preferred decoder path: %s",m_Runtime.decoderPath());

    Uint32 flags = SDL_RENDERER_ACCELERATED;
    if (params->enableVsync) flags |= SDL_RENDERER_PRESENTVSYNC;
    int driver = -1;
    for (int i=0;i<SDL_GetNumRenderDrivers();++i) {
        SDL_RendererInfo candidate{};
        if (SDL_GetRenderDriverInfo(i,&candidate)==0 && candidate.name && std::string(candidate.name)=="direct3d11") driver=i;
    }
    m_Renderer = SDL_CreateRenderer(params->window,driver,flags);
    if (!m_Renderer && driver>=0) m_Renderer=SDL_CreateRenderer(params->window,-1,flags);
    if (!m_Renderer) return fail(QString("SDL renderer creation: ") + SDL_GetError());
    // Limited black is only a preflight image. The first parsed live sequence
    // selects the actual texture colorspace on the main thread before upload.
    m_Texture = PyroWave::createLiveTexture(m_Renderer,m_Width,m_Height,PyroWave::YuvRange::Limited);
    if (!m_Texture)
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
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,"PyroWave SDL presentation initialized: %s I420; live sequence range pending",
                info.name ? info.name : "unknown");
    m_Gpu = std::make_unique<PyroWave::GpuPresentation>(m_Runtime);
    if(!m_Gpu->initialize(m_Renderer,m_Width,m_Height)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,"PyroWave GPU presentation unavailable: %s; retaining CPU I420 fallback",m_Gpu->error().c_str());
        m_Gpu.reset(); return false;
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,"PyroWave GPU presentation initialized: shared D3D11 R8 planes, timeline fences, 3 slots backend=LEGACY_D3D11_INTEROP");
    return true;
}
bool PyroWaveVideoDecoder::initializeCpu() {
    if(m_Failed || !m_Renderer || !m_Texture) return false;
    if(!m_Runtime.createDecoder(m_Width,m_Height)) return fail(QString::fromStdString(m_Runtime.error()));
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,"PyroWave CPU fallback decoder path: %s backend=CPU_I420_FALLBACK",m_Runtime.decoderPath());
    return true;
}

void PyroWaveVideoDecoder::retryRenderer() {
    if(m_RetryTimer || m_TestOnly) return;
    m_RetryTimer=SDL_AddTimer(16,[](Uint32,void*) -> Uint32 {
        // No decoder pointer crosses the timer callback; teardown can cancel the
        // timer and an already posted event is harmless to Session's null check.
        SDL_Event event{}; event.type=SDL_USEREVENT; event.user.code=SDL_CODE_FRAME_READY;
        SDL_PushEvent(&event); return 0;
    },nullptr);
    if(!m_RetryTimer) fail(QString("native render retry timer: ")+SDL_GetError());
}
void PyroWaveVideoDecoder::notifyNativeWindowEvent() {
    if(!m_Native) return;
    m_Native->windowChanged();
    std::lock_guard<std::mutex> guard(m_Mutex); wakeRenderer();
}
void PyroWaveVideoDecoder::wakeRenderer() {
    PyroWaveVulkan::wakeFrame(m_EventQueued,[] {
        SDL_Event event{};
        event.type = SDL_USEREVENT; event.user.code = SDL_CODE_FRAME_READY;
        return SDL_PushEvent(&event) == 1;
    });
}

int PyroWaveVideoDecoder::submitDecodeUnit(PDECODE_UNIT du) {
    {
        std::lock_guard<std::mutex> guard(m_Mutex);
        if (m_Failed) { wakeRenderer(); return DR_OK; }
        m_BackendSelection.beginLive();
        const uint64_t now = LiGetMicroseconds();
        if(m_Perf) {
            m_Perf->received(du->frameNumber);
            m_Perf->interval(PyroWavePerf::Interarrival,du->receiveTimeUs,m_Perf->lastArrival);
            m_Perf->ordered(PyroWavePerf::Reassembly,du->enqueueTimeUs,du->receiveTimeUs);
            m_Perf->ordered(PyroWavePerf::DecoderQueue,now,du->enqueueTimeUs);
            if(m_Perf->active(PyroWavePerf::nowUs())) {
                if(du->fullLength>0) m_Perf->increment(PyroWavePerf::VideoBytes,uint64_t(du->fullLength));
                if(du->frameHostProcessingLatency) m_Perf->record(PyroWavePerf::HostProcessing,uint64_t(du->frameHostProcessingLatency)*100);
            }
        }
        if (!m_Stats.measurementStartUs) m_Stats.measurementStartUs = now;
        if (m_LastFrameNumber && du->frameNumber > m_LastFrameNumber + 1) {
            const auto lost = du->frameNumber - m_LastFrameNumber - 1;
            m_Stats.networkDroppedFrames += lost; m_Stats.totalFrames += lost;
        }
        m_LastFrameNumber = du->frameNumber;
        ++m_Stats.receivedFrames; ++m_Stats.totalFrames;
        m_Stats.totalReassemblyTimeUs += PyroWave::elapsed(du->enqueueTimeUs,du->receiveTimeUs);
        m_Pipeline.decoderQueueUs += PyroWave::elapsed(now,du->enqueueTimeUs);
        if (du->fullLength > 0) m_Pipeline.bytes += du->fullLength;
        PyroWave::hostLatency(m_Stats,du->frameHostProcessingLatency);
    }
    PyroWave::Pixels pixels;
    std::size_t packets = 0;
    std::string rejection;
    PyroWave::DecodeTiming timing;
    int slot = -1;
    const auto assemblyStart = LiGetMicroseconds();
    uint64_t assemblyUs = 0;
    try {
        auto& bytes = m_FrameBytes;
        if (PyroWave::assembleLiveDecodeUnit(*du,bytes,rejection)) {
            assemblyUs = PyroWave::elapsed(LiGetMicroseconds(),assemblyStart);
            if (m_Gpu) {
                std::lock_guard<std::mutex> guard(m_Mutex);
                bool dropped;
                slot=m_Slots.reserve(dropped);
                if (dropped) {
                    ++m_Stats.pacerDroppedFrames;
                    if(m_Perf && m_Perf->active(PyroWavePerf::nowUs())) m_Perf->increment(PyroWavePerf::PresentationDrops);
                }
            }
            if (m_Gpu && slot<0) { fail("GPU frame slot invariant failed"); return DR_OK; }
            const bool decoded = m_Native ? m_Native->decode(bytes,packets,timing,du->frameNumber) : m_Gpu ? m_Gpu->decode(slot,bytes,packets,timing) : m_Runtime.decodeLive(bytes,pixels,packets,&timing);
            if (m_FirstSequence && m_Runtime.liveRange()) {
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,"PyroWave sequence: BT.709 %s-range, SDR 4:2:0",
                    *m_Runtime.liveRange() == PyroWave::YuvRange::Full ? "full" : "limited");
                m_FirstSequence = false;
            }
            if (!decoded) {
                if (m_Runtime.frameRejected()) rejection = m_Runtime.error();
                else { fail(QString::fromStdString(m_Runtime.error())); return DR_OK; }
            }
        }
    } catch (const std::exception& e) { fail(QString::fromUtf8(e.what())); return DR_OK; }
    if (!rejection.empty()) {
        // Includes reassembly rejection before any runtime packet submission.
        if(m_Perf && m_Perf->active(PyroWavePerf::nowUs())) {
            m_Perf->increment(PyroWavePerf::Rejected); m_Perf->event(PyroWavePerf::Reject,du->frameNumber);
        }
        m_Runtime.discardFrame();
        if (slot>=0) {
            std::lock_guard<std::mutex> guard(m_Mutex);
            m_Slots.cancel(slot);
        }
        if (++m_Rejected <= 5 || (m_Rejected % 256) == 0)
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,"PyroWave rejected frame %u: %s (rejected=%llu)",
                        du->frameNumber,rejection.c_str(),static_cast<unsigned long long>(m_Rejected));
        return DR_OK; // Independent frames: no IDR/hot-switch or partial recovery
    }
    {
        std::lock_guard<std::mutex> guard(m_Mutex);
        if (m_FirstFrame) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "First valid PyroWave decode unit: %d bytes, compatibility packets=%zu; first %s decode succeeded",
                        du->fullLength,packets,(m_Native || m_Gpu) ? "GPU output" : "I420");
            m_FirstFrame = false;
            m_PerformanceStartUs = LiGetMicroseconds();
        }
        if(m_Perf) {
            m_Perf->record(PyroWavePerf::Assembly,assemblyUs);
            m_Perf->record(PyroWavePerf::Preparation,timing.preparationUs);
            m_Perf->record((m_Native || m_Gpu) ? PyroWavePerf::GpuDecodeSubmission : PyroWavePerf::CpuDecodeReadback,timing.decodeUs);
            if(m_Perf->active(PyroWavePerf::nowUs())) m_Perf->increment(PyroWavePerf::Decoded);
            if(!m_Native) {
                m_Perf->interval(PyroWavePerf::PublicationInterval,PyroWavePerf::nowUs(),m_Perf->lastPublication);
                m_Perf->event(PyroWavePerf::Publish,du->frameNumber,slot);
            }
        }
        ++m_Stats.decodedFrames;
        m_Stats.totalDecodeTimeUs += timing.decodeUs;
        m_Pipeline.preparationUs += timing.preparationUs;
        m_Pipeline.assemblyUs += assemblyUs;
        if(m_Native) {
            // Native decode already published its bounded GPU slot.
        } else if (m_Gpu) {
            m_GpuFrameIds[slot]=du->frameNumber;
            m_GpuReadyUs[slot]=LiGetMicroseconds();
            if (m_Slots.publish(slot)) {
                ++m_Stats.pacerDroppedFrames;
                if(m_Perf && m_Perf->active(PyroWavePerf::nowUs())) m_Perf->increment(PyroWavePerf::PresentationDrops);
            }
        } else {
            if (!m_Pending.planes[0].empty()) {
                ++m_Stats.pacerDroppedFrames;
                if(m_Perf && m_Perf->active(PyroWavePerf::nowUs())) {
                    m_Perf->increment(PyroWavePerf::PresentationDrops); m_Perf->event(PyroWavePerf::Drop,m_PendingFrameId);
                }
            }
            m_PendingFrameId=du->frameNumber;
            m_Pending = std::move(pixels);
            m_PendingReadyUs=LiGetMicroseconds();
        }
        wakeRenderer();
    }
    // Once after ten seconds of live frames, and once after draining at shutdown.
    // Serialized with all codec API calls, outside the measured decode interval
    // and renderer mutex. No per-frame queries, timestamp reset or GPU drain.
    if (!m_ReportedStableTiming && PyroWave::elapsed(LiGetMicroseconds(),m_PerformanceStartUs)>=10000000) {
        m_ReportedStableTiming = true;
        reportGpuTiming();
    }
    return DR_OK;
}

void PyroWaveVideoDecoder::reportGpuTiming() {
    unsigned timestamps = 0;
    struct ReportContext { unsigned* timestamps; PyroWavePerf::Capture* capture; } context{&timestamps,m_Perf.get()};
    const auto callback = [](void* userdata,const char* message) {
        if (!message || !*message) return;
        // Upstream also reports memory budgets. Preserve that terminology too.
        const bool memory = std::strncmp(message,"Memory Heap ",12)==0;
        auto& context=*static_cast<ReportContext*>(userdata);
        if (!memory) ++*context.timestamps;
        if(context.capture) context.capture->codecReport(message);
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,"PyroWave %s: %s",
            memory ? "device performance" : "GPU timing",message);
    };
    if (!m_Runtime.reportPerformanceStats(callback,&context))
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,"PyroWave GPU timing unavailable: native reporting API/device unavailable");
    else if (!timestamps)
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,"PyroWave GPU timing unavailable: no resolved native timestamp intervals");
}

void PyroWaveVideoDecoder::configurePerf(bool testOnly) {
    if(testOnly) return;
    const auto directory=qEnvironmentVariable("ASTERIA_PERF_CAPTURE");
    if(directory.isEmpty()) return;
    // Existing, explicitly chosen directory; never create or upload anything.
    if(!QDir::isAbsolutePath(directory) || !QDir(directory).exists()) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,"Performance capture disabled: output must be an existing absolute directory"); return;
    }
    const auto seconds=[](const char* name,int fallback,int maximum) {
        const auto text=qEnvironmentVariable(name); if(text.isEmpty()) return fallback;
        bool okay=false; const auto value=text.toInt(&okay);
        return okay && value>=0 && value<=maximum ? value : -1;
    };
    const int warmup=seconds("ASTERIA_PERF_WARMUP_SECONDS",10,3600);
    const int duration=seconds("ASTERIA_PERF_DURATION_SECONDS",60,86400);
    if(warmup<0 || duration<=0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,"Performance capture disabled: invalid warmup/duration"); return;
    }
    try {
        m_Perf=std::make_unique<PyroWavePerf::Capture>();
        m_Perf->warmup=uint64_t(warmup)*1000000; m_Perf->duration=uint64_t(duration)*1000000;
        m_PerfDirectory=directory;
    } catch(...) { m_Perf.reset(); SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,"Performance capture allocation unavailable"); }
}

void PyroWaveVideoDecoder::exportPerf() noexcept {
    if(!m_Perf || !m_Perf->start) return;
    try {
        auto report=PyroWavePerf::serialize(*m_Perf);
        auto metrics=report["metrics"].toObject();
        auto submission=metrics["newFrameSubmissionInterval"].toObject();
        submission["scope"]=m_Native ? "CPU:successive_new_video_vkQueueSubmit_success_callbacks" : "CPU:successive_new_video_SDL_RenderPresent_returns";
        metrics["newFrameSubmissionInterval"]=submission;
        auto pickup=metrics["frameAgeAtPickup"].toObject();
        pickup["clock"]=m_Native ? "steady_clock" : "common-c monotonic";
        metrics["frameAgeAtPickup"]=pickup;
        report["metrics"]=metrics;
        report["sourceRevision"]=ASTERIA_PYROWAVE_SOURCE_REVISION;
        report["architecture"]=QSysInfo::buildCpuArchitecture();
        report["gpu"]=m_PerfGpu; report["driver"]=QJsonValue(QJsonValue::Null);
        report["backend"]=m_PerfBackend; report["decoderPath"]=m_PerfDecoderPath;
        report["codec"]=QJsonObject{{"name","PyroWave"},{"commit",PyroWave::CodecCommit},
            {"bitstreamId",PyroWave::BitstreamId},{"apiVersion","0.6.0"}};
        report["video"]=QJsonObject{{"width",m_Width},{"height",m_Height},{"targetFps",m_TargetFps},
            {"requestedBitrateKbps",m_BitrateKbps},{"vsync",m_Vsync}};
        report["systemResources"]=QJsonObject{{"processCpuPercent",QJsonValue(QJsonValue::Null)},
            {"processWorkingSetBytes",QJsonValue(QJsonValue::Null)},{"processGpuUtilizationPercent",QJsonValue(QJsonValue::Null)},
            {"gpuMemoryBytes",QJsonValue(QJsonValue::Null)},{"batteryDischargeWatts",QJsonValue(QJsonValue::Null)},
            {"thermalState",QJsonValue(QJsonValue::Null)},{"gpuClockMHz",QJsonValue(QJsonValue::Null)}};
        const auto seconds=report["capture"].toObject()["durationSeconds"].toDouble();
        const bool observedAvailable=seconds>0 && !report["capture"].toObject()["counterOverflow"].toBool();
        report["observed"]=QJsonObject{
            {"incomingFps",observedAvailable ? QJsonValue(double(m_Perf->counts[PyroWavePerf::Received].load())/seconds) : QJsonValue(QJsonValue::Null)},
            {"submissionFps",observedAvailable ? QJsonValue(double(m_Perf->counts[PyroWavePerf::Submitted].load())/seconds) : QJsonValue(QJsonValue::Null)},
            {"videoPayloadMbps",observedAvailable ? QJsonValue(double(m_Perf->counts[PyroWavePerf::VideoBytes].load())*8/seconds/1000000) : QJsonValue(QJsonValue::Null)},
            {"jitterDrops",QJsonValue(QJsonValue::Null)}};
        QJsonArray codecReports;
        for(size_t i=0;i<m_Perf->codecReportCount;++i) codecReports.append(QString::fromUtf8(m_Perf->codecReports[i].data()));
        report["nativeCodecReports"]=QJsonObject{{"scope","cumulative device reports; includes warmup; native text/units"},
            {"reports",codecReports},{"truncated",m_Perf->codecReportsTruncated}};
        Session::get()->retainPerformanceReport(m_PerfDirectory,report);
    } catch(...) { SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,"Performance capture export unavailable; stream cleanup continues"); }
}

void PyroWaveVideoDecoder::updateStats() {
    VIDEO_STATS stats{};
    PyroWave::PipelineStats pipeline;
    const auto now = LiGetMicroseconds();
    if (now - m_StatsTime < 1000000) return;
    {
        std::lock_guard<std::mutex> guard(m_Mutex);
        if (!m_Stats.measurementStartUs || PyroWave::elapsed(now,m_Stats.measurementStartUs)<1000000) return;
        m_StatsTime = now;
        // Like FFmpeg, display the previous and active approximately one-second windows.
        PyroWave::addStats(m_LastStats,stats);
        PyroWave::addStats(m_Stats,stats);
        pipeline.bytes=m_LastPipeline.bytes+m_Pipeline.bytes;
        pipeline.preparationUs=m_LastPipeline.preparationUs+m_Pipeline.preparationUs;
        pipeline.assemblyUs=m_LastPipeline.assemblyUs+m_Pipeline.assemblyUs;
        pipeline.decoderQueueUs=m_LastPipeline.decoderQueueUs+m_Pipeline.decoderQueueUs;
        PyroWave::addStats(m_Stats,m_StreamStats);
        m_StreamPipeline.bytes+=m_Pipeline.bytes;
        m_StreamPipeline.preparationUs+=m_Pipeline.preparationUs;
        m_StreamPipeline.assemblyUs+=m_Pipeline.assemblyUs;
        m_StreamPipeline.decoderQueueUs+=m_Pipeline.decoderQueueUs;
        m_LastStats=m_Stats; m_LastPipeline=m_Pipeline;
        m_Stats={}; m_Stats.measurementStartUs=now; m_Pipeline={};
    }
    if (!LiGetEstimatedRttInfo(&stats.lastRtt,&stats.lastRttVariance)) stats.lastRtt=stats.lastRttVariance=0;
    if(m_Perf && stats.lastRtt) m_Perf->record(PyroWavePerf::Rtt,uint64_t(stats.lastRtt)*1000);
    const auto text=PyroWave::formatStats(stats,pipeline,m_Width,m_Height,now,bool(m_Native || m_Gpu));
    auto& overlays = Session::get()->getOverlayManager();
    if (overlays.isOverlayEnabled(Overlay::OverlayDebug)) overlays.updateOverlayText(Overlay::OverlayDebug,text.c_str());
}

bool PyroWaveVideoDecoder::renderOverlays() {
    bool changed=false;
    auto& manager = Session::get()->getOverlayManager();
    int width=0,height=0;
    SDL_GetRendererOutputSize(m_Renderer,&width,&height);
    for (int i=0;i<Overlay::OverlayMax;++i) {
        const auto type = static_cast<Overlay::OverlayType>(i);
        if (!manager.isOverlayEnabled(type)) continue;
        if (auto surface = manager.getUpdatedOverlaySurface(type)) {
            changed=true;
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
    return changed;
}

void PyroWaveVideoDecoder::renderFrameOnMainThread() {
    PyroWavePerf::Timer renderTimer(m_Perf.get(),PyroWavePerf::RenderLoop);
    if(m_Perf) m_Perf->interval(PyroWavePerf::ScheduleInterval,PyroWavePerf::nowUs(),m_Perf->lastSchedule);
    if(m_RetryTimer) { SDL_RemoveTimer(m_RetryTimer); m_RetryTimer=0; }
    if(m_Native) {
        { std::lock_guard<std::mutex> guard(m_Mutex); m_EventQueued=false; if(m_Failed) return; }
        try {
            updateStats();
            auto& manager=Session::get()->getOverlayManager();
            for(unsigned i=0;i<Overlay::OverlayMax;++i) {
                const auto type=static_cast<Overlay::OverlayType>(i);
                std::unique_ptr<SDL_Surface,decltype(&SDL_FreeSurface)> surface(manager.getUpdatedOverlaySurface(type),SDL_FreeSurface);
                m_Native->updateOverlay(i,surface.get(),manager.isOverlayEnabled(type));
            }
            const auto start=LiGetMicroseconds(); const auto result=m_Native->render();
            const uint64_t nativeDrops=m_Native->presentationDrops();
            { std::lock_guard<std::mutex> guard(m_Mutex);
              m_Stats.pacerDroppedFrames+=uint32_t(nativeDrops-m_LastNativeDrops); m_LastNativeDrops=nativeDrops; }
            if(result.newFrame) {
                std::lock_guard<std::mutex> guard(m_Mutex);
                ++m_Stats.renderedFrames; m_Stats.totalPacerTimeUs+=result.readyUs;
                m_Stats.totalRenderTimeUs+=PyroWave::elapsed(LiGetMicroseconds(),start);
            }
            if(result.retry) retryRenderer();
        } catch(const std::exception& e) { fail(QString::fromUtf8(e.what())); }
        return;
    }
    PyroWave::Pixels pixels;
    int slot=-1,displayed=-1;
    uint64_t readyUs=0; uint32_t frameId=0; bool overlayChanged=false;
    {
        std::lock_guard<std::mutex> guard(m_Mutex);
        m_EventQueued = false;
        if (m_Failed) return; // Session reads getError() and tears down normally
        if (m_Gpu) {
            slot=m_Slots.take(); displayed=slot>=0 ? slot : m_Slots.current();
            if (slot>=0) readyUs=m_GpuReadyUs[slot];
        }
        else readyUs=m_PendingReadyUs;
        frameId=slot>=0 ? m_GpuFrameIds[slot] : m_PendingFrameId;
        overlayChanged=m_OverlayChanged; m_OverlayChanged=false;
        pixels = std::move(m_Pending);
        m_Pending = {};
    }
    const auto dequeued = LiGetMicroseconds();
    const bool newFrame=slot>=0 || !pixels.planes[0].empty();
    if(m_Perf && newFrame) m_Perf->ordered(PyroWavePerf::PickupAge,dequeued,readyUs);
    updateStats();
    const auto start = LiGetMicroseconds();
    if (!pixels.planes[0].empty()) {
        if (!m_TextureRange) {
            auto texture = PyroWave::createLiveTexture(m_Renderer,m_Width,m_Height,pixels.range);
            if (!texture) { fail(QString("SDL BT.709 range texture: ") + SDL_GetError()); return; }
            SDL_DestroyTexture(m_Texture);
            m_Texture = texture;
            m_TextureRange = pixels.range;
        }
        if (SDL_UpdateYUVTexture(m_Texture,nullptr,pixels.planes[0].data(),m_Width,
            pixels.planes[1].data(),m_Width/2,pixels.planes[2].data(),m_Width/2) != 0) {
            fail(QString("SDL I420 upload: ") + SDL_GetError()); return;
        }
        m_HaveTextureFrame = true;
    }
    if (!m_HaveTextureFrame && displayed<0) return;
    SDL_Rect source{0,0,m_Width,m_Height}, destination{0,0,0,0};
    if (SDL_GetRendererOutputSize(m_Renderer,&destination.w,&destination.h) != 0) {
        fail(QString("SDL output size: ") + SDL_GetError()); return;
    }
    if (!destination.w || !destination.h) {
        if (slot>=0 || !pixels.planes[0].empty()) {
            std::lock_guard<std::mutex> guard(m_Mutex);
            if (slot>=0) m_Slots.cancel(slot);
            ++m_Stats.pacerDroppedFrames;
            if(m_Perf && m_Perf->active(PyroWavePerf::nowUs())) {
                m_Perf->increment(PyroWavePerf::PresentationDrops); m_Perf->event(PyroWavePerf::Drop,frameId,slot);
            }
        }
        return;
    }
    if (m_Gpu && !m_Gpu->beginRender(displayed)) { fail(QString::fromStdString(m_Gpu->error())); return; }
    StreamUtils::scaleSourceToDestinationSurface(&source,&destination);
    if (SDL_RenderClear(m_Renderer) != 0 ||
        SDL_RenderCopy(m_Renderer,m_Gpu ? m_Gpu->texture(displayed) : m_Texture,nullptr,&destination) != 0) {
        fail(QString("SDL presentation: ") + SDL_GetError()); return;
    }
    const bool overlayWork=renderOverlays() || overlayChanged;
    SDL_RenderPresent(m_Renderer);
    if(m_Perf) m_Perf->submitted(newFrame,overlayWork,frameId,slot);
    if (m_Gpu && !m_Gpu->endRender(displayed)) { fail(QString::fromStdString(m_Gpu->error())); return; }
    if (!pixels.planes[0].empty() || slot>=0) {
        std::lock_guard<std::mutex> guard(m_Mutex);
        if (slot>=0) m_Slots.displayed(slot);
        m_Stats.totalPacerTimeUs += PyroWave::elapsed(dequeued,readyUs);
        ++m_Stats.renderedFrames;
        m_Stats.totalRenderTimeUs += LiGetMicroseconds() - start;
    }
}

void PyroWaveVideoDecoder::notifyOverlayUpdated(Overlay::OverlayType) {
    std::lock_guard<std::mutex> guard(m_Mutex);
    m_OverlayChanged=true;
    wakeRenderer();
}

bool PyroWaveVideoDecoder::notifyWindowChanged(PWINDOW_STATE_CHANGE_INFO info) {
    if(m_Native) { notifyNativeWindowEvent(); return true; }
    // Output size and aspect fit are recalculated on the main thread each render.
    return !(info->stateChangeFlags & ~(WINDOW_STATE_CHANGE_SIZE | WINDOW_STATE_CHANGE_DISPLAY));
}

void PyroWaveVideoDecoder::logNativeSummary() {
    const auto& d=m_Native->diagnostics;
    VIDEO_STATS stats=m_StreamStats; PyroWave::addStats(m_Stats,stats);
    QJsonObject o{{"backend","NATIVE_VULKAN"},{"presentationPath","GPU_DECODE_CALLER_YUV_SHADER_SWAPCHAIN"},
        {"preferredDecoderPath",QString::fromStdString(d.preferredPath)},{"actualDecoderPath",QString::fromStdString(d.actualPath)},
        {"selectedGpu",QString::fromStdString(d.gpu)},{"runtimeCommit",PyroWave::CodecCommit},{"bitstreamId",PyroWave::BitstreamId},
        {"apiVersion","0.6.0"},{"graniteCommit","b6cffd5ce81f540f0855e6778428483e14763d9b"},
        {"sourceRevision",ASTERIA_PYROWAVE_SOURCE_REVISION},{"testOnly",m_TestOnly},
        {"borrowedInstanceMatch",d.borrowedMatch},{"borrowedPhysicalDeviceMatch",d.borrowedMatch},{"borrowedDeviceMatch",d.borrowedMatch},
        {"externalMemoryHandles",0},{"externalSemaphoreHandles",0},{"d3d11Resources",0},{"slots",3},
        {"validation",d.validationActive ? "ENABLED" : "SKIP"},{"cleanupOkay",d.cleanupOkay}};
    const auto number=[&o](const char* key,uint64_t value) { o[key]=qint64(value); };
    number("receivedFrames",stats.receivedFrames); number("decodedFrames",d.decoded); number("renderedFrames",d.rendered);
    number("networkDrops",stats.networkDroppedFrames); number("presentationDrops",d.presentationDrops);
    number("nativeRetiredDrops",d.retiredDrops); number("cpuYuvReadbackFrames",d.cpuReadbacks);
    number("swapchainRecreations",d.recreations); number("overlayUploads",d.overlayUploads); number("overlayRedraws",d.overlayRedraws);
    number("retainedFrameRedraws",d.retainedFrameRedraws);
    number("fatalPresenterErrors",d.fatalErrors); number("slotReuse",d.slotReuse); number("decodeTimelineWaits",d.decodeWaits);
    number("consumerTimelineSignals",d.consumerSignals); number("timelineErrors",d.timelineErrors);
    number("validationErrors",d.validationErrors); number("validationWarnings",d.validationWarnings);
    number("decodeApiSubmissionUs",d.decodeSubmitUs); number("presenterQueueSubmitUs",d.queueSubmitUs);
    number("queuePresentCallUs",d.presentCallUs); number("renderLoopCpuUs",d.renderLoopUs);
    number("assemblyUs",m_StreamPipeline.assemblyUs+m_Pipeline.assemblyUs);
    number("parserPreparationUs",m_StreamPipeline.preparationUs+m_Pipeline.preparationUs);
    number("decoderQueueWaitUs",m_StreamPipeline.decoderQueueUs+m_Pipeline.decoderQueueUs);
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,"PYROWAVE_NATIVE_STREAM_SUMMARY %s",QJsonDocument(o).toJson(QJsonDocument::Compact).constData());
}
