#pragma once
#ifndef PYROWAVE_EXPERIMENTAL
#error "PyroWave runtime must only be compiled in an explicit experimental build"
#endif

#include "pyrowave_frame.h"
#include "pyrowave_pixels.h"
#include <vulkan/vulkan_core.h>
#include <pyrowave/pyrowave.h>
#include <filesystem>
#include <optional>

namespace PyroWave {
struct DecodeTiming {
    uint64_t preparationUs = 0;
    // CPU API duration: submission only for GPU output; decode + readback for CPU output.
    uint64_t decodeUs = 0;
};
// Serialized P0/live owner. Module outlives every device/decoder and API call.
// No global instance and no startup load. Missing DLL is a normal false result.
class Runtime {
public:
    Runtime() = default;
    ~Runtime();
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;
    bool load(const std::filesystem::path& dependencyDirectory);
    // GPU output uses the device recommendation; CPU I420 defaults to compute.
    bool createDecoder(int width, int height, bool preferGpuPath = false);
    const char* decoderPath() const { return m_FragmentPath ? "fragment" : "compute"; }
    // Best-effort, serialized with decode; native callback text/units are unchanged.
    bool reportPerformanceStats(pyrowave_message_cb callback, void* userdata, bool reset = false);
    bool decode(const std::vector<std::uint8_t>& container, Pixels& output);
    bool decodeLive(const std::vector<std::uint8_t>& container, Pixels& output,
                    std::size_t& packetCount, DecodeTiming* timing = nullptr);
    bool frameRejected() const { return m_FrameRejected; }
    std::optional<YuvRange> liveRange() const { return m_LiveRange; }
    void discardFrame();
    void resetDecoder();
    void close();
    const std::string& error() const { return m_Error; }
    const std::string& deviceDescription() const { return m_DeviceDescription; }

    // Offline proof only: resolves encoder exports lazily and generates a frame
    // with this exact loaded codec. Does not accept host or Session inputs.
    bool generateProofFrame(std::vector<std::uint8_t>& container,
                            Framing framing = Framing::Compatibility);
    bool encodeProofPixels(const Pixels& pixels, std::vector<std::uint8_t>& container,
                           Framing framing = Framing::Compatibility);
#ifdef PYROWAVE_VULKAN_SHARED_DEVICE
    // Only compiled by the isolated offline project; caller retains all Vulkan
    // create-info storage, queue userdata and handles until close() finishes.
    bool borrowDevice(const pyrowave_device_create_info& info);
    bool decodeNative(const std::vector<std::uint8_t>& container,
                      const pyrowave_gpu_buffers& buffers,
                      const pyrowave_gpu_sync_operation& acquire,
                      const pyrowave_gpu_sync_operation& release);
#endif

private:
    friend class GpuPresentation;
    friend struct RuntimeTestAccess;
    bool decodeImpl(const std::vector<std::uint8_t>& container, Pixels& output,
                    bool live, std::size_t* packetCount, DecodeTiming* timing = nullptr,
                    const pyrowave_gpu_buffers* gpu = nullptr,
                    const pyrowave_gpu_sync_operation* acquire = nullptr,
                    const pyrowave_gpu_sync_operation* release = nullptr);
    bool m_FrameRejected = false;
    std::optional<YuvRange> m_LiveRange;
    bool fail(const std::string& reason);
    bool check(pyrowave_result result, const char* operation);
    void* symbol(const char* name);
    struct Api {
        decltype(&pyrowave_get_api_version) version = nullptr;
        decltype(&pyrowave_create_default_device) createDevice = nullptr;
        decltype(&pyrowave_device_destroy) destroyDevice = nullptr;
        decltype(&pyrowave_device_get_vk_device_handles) deviceHandles = nullptr;
        decltype(&pyrowave_decoder_create) createDecoder = nullptr;
        decltype(&pyrowave_decoder_device_prefers_fragment_path) prefersFragment = nullptr;
        decltype(&pyrowave_device_report_performance_stats) reportStats = nullptr;
        decltype(&pyrowave_decoder_decode_gpu_buffer) decodeGpu = nullptr;
        decltype(&pyrowave_decoder_destroy) destroyDecoder = nullptr;
        decltype(&pyrowave_decoder_clear) clear = nullptr;
        decltype(&pyrowave_decoder_push_packet) push = nullptr;
        decltype(&pyrowave_decoder_decode_is_ready) ready = nullptr;
        decltype(&pyrowave_decoder_decode_cpu_buffer_synchronous) decode = nullptr;
    } m_Api;
    void* m_Module = nullptr;
    void* m_Vulkan = nullptr;
    pyrowave_device m_Device = nullptr;
    pyrowave_decoder m_Decoder = nullptr;
    bool m_FragmentPath = false;
    Frame m_Frame; // Reuse bounded packet/record/validation metadata across frames.
    int m_Width = 0;
    int m_Height = 0;
    std::string m_Error;
    std::string m_DeviceDescription;
};
}
