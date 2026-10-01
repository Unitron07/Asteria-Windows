#pragma once
#ifndef PYROWAVE_EXPERIMENTAL
#error "PyroWave runtime must only be compiled in an explicit experimental build"
#endif

#include "pyrowave_frame.h"
#include "pyrowave_pixels.h"
#include <vulkan/vulkan_core.h>
#include <pyrowave.h>
#include <filesystem>

namespace PyroWave {
// Serialized P0/live owner. Module outlives every device/decoder and API call.
// No global instance and no startup load. Missing DLL is a normal false result.
class Runtime {
public:
    Runtime() = default;
    ~Runtime();
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;
    bool load(const std::filesystem::path& dependencyDirectory);
    bool createDecoder(int width, int height);
    bool decode(const std::vector<std::uint8_t>& container, Pixels& output);
    bool decodeLive(const std::vector<std::uint8_t>& container, Pixels& output,
                    std::size_t& packetCount);
    bool frameRejected() const { return m_FrameRejected; }
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

private:
    bool decodeImpl(const std::vector<std::uint8_t>& container, Pixels& output,
                    bool live, std::size_t* packetCount);
    bool m_FrameRejected = false;
    bool fail(const std::string& reason);
    bool check(pyrowave_result result, const char* operation);
    void* symbol(const char* name);
    struct Api {
        decltype(&pyrowave_get_api_version) version = nullptr;
        decltype(&pyrowave_create_default_device) createDevice = nullptr;
        decltype(&pyrowave_device_destroy) destroyDevice = nullptr;
        decltype(&pyrowave_device_get_vk_device_handles) deviceHandles = nullptr;
        decltype(&pyrowave_decoder_create) createDecoder = nullptr;
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
    int m_Width = 0;
    int m_Height = 0;
    std::string m_Error;
    std::string m_DeviceDescription;
};
}
