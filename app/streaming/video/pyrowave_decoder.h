#pragma once
#include "decoder.h"
#include "overlaymanager.h"
#include "pyrowave_runtime.h"
#include "pyrowave_gpu.h"
#include "pyrowave_queue.h"
#include "pyrowave_stats.h"
#include <mutex>

// Decode on common-c's VideoDec thread; all SDL resources belong to the SDL main
// thread. One replaceable pending frame bounds queue memory and render latency.
class PyroWaveVideoDecoder : public IVideoDecoder, public Overlay::IOverlayRenderer {
public:
    ~PyroWaveVideoDecoder() override;
    bool initialize(PDECODER_PARAMETERS params) override;
    bool isHardwareAccelerated() override { return true; }
    bool isAlwaysFullScreen() override { return false; }
    bool isHdrSupported() override { return false; }
    int getDecoderCapabilities() override { return 0; } // Dedicated common-c decoder thread
    int getDecoderColorspace() override { return COLORSPACE_REC_709; }
    // Negotiation preference only; parsed sequence metadata controls presentation.
    int getDecoderColorRange() override { return COLOR_RANGE_LIMITED; }
    QSize getDecoderMaxResolution() override { return QSize(3840,2160); }
    int submitDecodeUnit(PDECODE_UNIT du) override;
    void renderFrameOnMainThread() override;
    void setHdrMode(bool) override {} // Desktop HDR state never changes this SDR path
    bool notifyWindowChanged(PWINDOW_STATE_CHANGE_INFO info) override;
    void notifyOverlayUpdated(Overlay::OverlayType) override;
    QString getError() override;

private:
    bool fail(const QString& reason);
    void wakeRenderer(); // m_Mutex held
    void updateStats();
    void renderOverlays();
    PyroWave::Runtime m_Runtime;
    SDL_Renderer* m_Renderer = nullptr;
    SDL_Texture* m_Texture = nullptr;
    SDL_Texture* m_OverlayTextures[Overlay::OverlayMax] = {};
    std::mutex m_Mutex;
    PyroWave::Pixels m_Pending;
    uint64_t m_PendingReadyUs = 0;
    std::unique_ptr<PyroWave::GpuPresentation> m_Gpu;
    PyroWave::FrameSlots m_Slots;
    uint64_t m_GpuReadyUs[PyroWave::FrameSlots::Count] = {};
    VIDEO_STATS m_Stats = {}, m_LastStats = {};
    PyroWave::PipelineStats m_Pipeline, m_LastPipeline;
    QString m_Error;
    int m_Width = 0, m_Height = 0;
    uint32_t m_LastFrameNumber = 0;
    uint64_t m_StatsTime = 0, m_Rejected = 0;
    bool m_TestOnly = true, m_EventQueued = false, m_Failed = false;
    bool m_FirstFrame = true, m_HaveTextureFrame = false;
    bool m_FirstSequence = true;
    std::optional<PyroWave::YuvRange> m_TextureRange;
};
