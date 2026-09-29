#include "pyrowave_runtime.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <memory>
#include <new>

static_assert(PYROWAVE_API_VERSION_MAJOR == 0 && PYROWAVE_API_VERSION_MINOR == 6 &&
              PYROWAVE_API_VERSION_PATCH == 0, "P0 requires pinned API 0.6.0 headers");

namespace PyroWave {
Runtime::~Runtime() { close(); }
bool Runtime::fail(const std::string& reason) {
    m_Error = reason;
    std::cerr << "PyroWave P0: " << reason << '\n';
    OutputDebugStringA(("PyroWave P0: " + reason + "\n").c_str());
    return false;
}
bool Runtime::check(pyrowave_result result, const char* operation) {
    return result == PYROWAVE_SUCCESS || fail(std::string(operation) + " failed: " + std::to_string(result));
}
void* Runtime::symbol(const char* name) {
    auto p = GetProcAddress(static_cast<HMODULE>(m_Module), name);
    if (!p) fail(std::string("missing C API export: ") + name);
    return reinterpret_cast<void*>(p);
}
bool Runtime::load(const std::filesystem::path& directory) {
    close();
    m_Error.clear();
    if (!directory.is_absolute()) return fail("dependency directory must be absolute");
    std::error_code ec;
    const auto dll = std::filesystem::canonical(directory / L"libpyrowave-shared-0.dll", ec);
    if (ec) return fail("runtime DLL absent or inaccessible: " + ec.message());
    // Explicit file; dependencies search only its directory and Windows system32.
    // Never use PATH/CWD or mutate the process-wide DLL search policy.
    m_Module = LoadLibraryExW(dll.c_str(), nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!m_Module) return fail("LoadLibraryExW failed with Windows error " + std::to_string(GetLastError()));
    m_Api.version = reinterpret_cast<decltype(m_Api.version)>(symbol("pyrowave_get_api_version"));
    if (!m_Api.version) { close(); return false; }
    std::uint32_t major=0, minor=0, patch=0;
    m_Api.version(&major,&minor,&patch);
    if (major != 0 || minor != 6 || patch != 0) {
        fail("expected API 0.6.0, got " + std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch));
        close(); return false;
    }
#define RESOLVE(member, name) m_Api.member = reinterpret_cast<decltype(m_Api.member)>(symbol(#name)); if (!m_Api.member) { close(); return false; }
    RESOLVE(createDevice, pyrowave_create_default_device)
    RESOLVE(destroyDevice, pyrowave_device_destroy)
    RESOLVE(deviceHandles, pyrowave_device_get_vk_device_handles)
    RESOLVE(createDecoder, pyrowave_decoder_create)
    RESOLVE(destroyDecoder, pyrowave_decoder_destroy)
    RESOLVE(clear, pyrowave_decoder_clear)
    RESOLVE(push, pyrowave_decoder_push_packet)
    RESOLVE(ready, pyrowave_decoder_decode_is_ready)
    RESOLVE(decode, pyrowave_decoder_decode_cpu_buffer_synchronous)
#undef RESOLVE
    return true;
}
void Runtime::resetDecoder() {
    if (m_Decoder) m_Api.destroyDecoder(m_Decoder);
    m_Decoder = nullptr; m_Width = m_Height = 0;
}
void Runtime::close() {
    resetDecoder();
    if (m_Device) m_Api.destroyDevice(m_Device);
    m_Device = nullptr;
    if (m_Module) FreeLibrary(static_cast<HMODULE>(m_Module));
    m_Module = nullptr; m_Api = {};
    if (m_Vulkan) FreeLibrary(static_cast<HMODULE>(m_Vulkan));
    m_Vulkan = nullptr;
    m_DeviceDescription.clear();
}
bool Runtime::createDecoder(int width, int height) {
    resetDecoder();
    m_Error.clear();
    if (!m_Module) return fail("runtime is not loaded");
    // Fixed P0 extent. Broader stream dimensions/color contracts are a later gate.
    if (width != 1920 || height != 1080) return fail("P0 decoder accepts only 1920x1080 SDR 420");
    // volk at this pin calls LoadLibraryA("vulkan-1.dll"). Preload only the system
    // loader so that indirect call cannot select a loader from CWD or PATH.
    if (!m_Vulkan) {
        m_Vulkan = LoadLibraryExW(L"vulkan-1.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!m_Vulkan) return fail("native system Vulkan loader unavailable, Windows error " + std::to_string(GetLastError()));
    }
    if (!m_Device && !check(m_Api.createDevice(&m_Device), "device creation")) { close(); return false; }
    if (!m_Device) return fail("device creation returned a null device");
    auto getProc = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        GetProcAddress(static_cast<HMODULE>(m_Vulkan),"vkGetInstanceProcAddr"));
    VkInstance instance=VK_NULL_HANDLE; VkPhysicalDevice physical=VK_NULL_HANDLE;
    m_Api.deviceHandles(m_Device,&instance,&physical,nullptr);
    if (!getProc || !instance || !physical) return fail("cannot inspect selected Vulkan adapter");
    auto getProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
        getProc(instance,"vkGetPhysicalDeviceProperties"));
    if (!getProperties) return fail("Vulkan physical-device properties entry point unavailable");
    VkPhysicalDeviceProperties properties{};
    getProperties(physical,&properties);
    m_DeviceDescription=std::string(properties.deviceName) + " vendorID=" + std::to_string(properties.vendorID) +
        " deviceID=" + std::to_string(properties.deviceID) + " driverVersion=" + std::to_string(properties.driverVersion) +
        " apiVersion=" + std::to_string(properties.apiVersion);
    pyrowave_decoder_create_info info{};
    info.device=m_Device; info.width=width; info.height=height;
    info.chroma=PYROWAVE_CHROMA_SUBSAMPLING_420; info.fragment_path=false;
    if (!check(m_Api.createDecoder(&info,&m_Decoder),"decoder creation")) { resetDecoder(); return false; }
    if (!m_Decoder) return fail("decoder creation returned a null decoder");
    m_Width=width; m_Height=height;
    return true;
}
bool Runtime::decode(const std::vector<std::uint8_t>& container, Pixels& output) {
    output = {};
    m_Error.clear();
    if (!m_Decoder) return fail("decoder is not created");
    m_Api.clear(m_Decoder);
    // Always clear on success, parser failure, push failure, not-ready and decode failure.
    const auto clear = [this](void*) { m_Api.clear(m_Decoder); };
    std::unique_ptr<void, decltype(clear)> cleanup(this, clear);
    Frame frame; std::string error;
    if (!parseFrame(container.data(),container.size(),container.size(),frame,error)) return fail(error);
    for (const auto& p : frame.packets)
        if (!check(m_Api.push(m_Decoder,container.data()+p.offset,p.size),"packet push")) return false;
    if (!m_Api.ready(m_Decoder,false)) return fail("complete offline frame is not decode-ready");
    Pixels pixels; pixels.width=m_Width; pixels.height=m_Height;
    pyrowave_cpu_buffer buffer{};
    buffer.width=m_Width; buffer.height=m_Height; buffer.format=PYROWAVE_CPU_BUFFER_FORMAT_YUV420P;
    try {
        for (int p=0;p<3;++p) {
            const std::size_t w=p ? m_Width/2 : m_Width;
            const std::size_t h=p ? m_Height/2 : m_Height;
            // Fixed dimensions validated before device/allocation; <= 3,110,400 bytes total.
            pixels.planes[p].resize(w*h);
            buffer.data[p]=pixels.planes[p].data();
            buffer.row_stride_in_bytes[p]=w; buffer.plane_size_in_bytes[p]=w*h;
        }
    } catch (const std::bad_alloc&) { return fail("I420 output allocation failed"); }
    if (!check(m_Api.decode(m_Decoder,&buffer),"synchronous CPU decode")) return false;
    output=std::move(pixels);
    return true;
}
bool Runtime::generateProofFrame(std::vector<std::uint8_t>& container) {
    container.clear();
    if (!m_Device || !m_Decoder) return fail("create P0 decoder/device before generating fixture");
#define ENCODER(name) auto name = reinterpret_cast<decltype(&pyrowave_encoder_##name)>(symbol("pyrowave_encoder_" #name)); if (!name) return false
    ENCODER(create); ENCODER(destroy); ENCODER(encode_cpu_synchronous);
    ENCODER(compute_num_packets); ENCODER(packetize);
#undef ENCODER
    pyrowave_encoder_create_info info{};
    info.device=m_Device; info.width=m_Width; info.height=m_Height; info.chroma=PYROWAVE_CHROMA_SUBSAMPLING_420;
    pyrowave_encoder encoder=nullptr;
    if (!check(create(&info,&encoder),"proof encoder creation")) {
        if (encoder) destroy(encoder);
        return false;
    }
    if (!encoder) return fail("proof encoder returned null");
    const auto destroyEncoder = [destroy](pyrowave_encoder_opaque* e) { destroy(e); };
    std::unique_ptr<pyrowave_encoder_opaque,decltype(destroyEncoder)> cleanup(encoder,destroyEncoder);
    try {
        std::vector<std::uint8_t> planes[3];
        pyrowave_cpu_buffer b{};
        b.width=m_Width; b.height=m_Height; b.format=PYROWAVE_CPU_BUFFER_FORMAT_YUV420P;
        for (int p=0;p<3;++p) {
            const std::size_t w=p ? m_Width/2 : m_Width, h=p ? m_Height/2 : m_Height;
            planes[p].resize(w*h);
            // Deterministic limited-range gray luma ramp; neutral chroma.
            for (std::size_t y=0;y<h;++y) for (std::size_t x=0;x<w;++x)
                planes[p][y*w+x]=p ? 128 : std::uint8_t(16+219*x/(w-1));
            b.data[p]=planes[p].data(); b.row_stride_in_bytes[p]=w; b.plane_size_in_bytes[p]=w*h;
        }
        // Leave space for PYRW packet-length overhead below the full-frame safety cap.
        const pyrowave_rate_control rate{800000};
        if (!check(encode_cpu_synchronous(encoder,&b,&rate),"proof encode")) return false;
        constexpr std::size_t boundary=1200;
        std::size_t count=0;
        if (!check(compute_num_packets(encoder,boundary,&count),"proof packet count")) return false;
        if (!count || count>MaxPackets) return fail("proof packet count outside safety limit");
        std::vector<pyrowave_packet> packets(count);
        std::vector<std::uint8_t> bitstream(MaxFrameBytes);
        std::size_t actual=count;
        if (!check(packetize(encoder,packets.data(),boundary,&actual,bitstream.data(),bitstream.size()),"proof packetize")) return false;
        if (actual!=count) return fail("proof packetizer count mismatch");
        std::size_t total=8;
        for (const auto& p : packets) {
            if (!p.size || p.offset>bitstream.size() || p.size>bitstream.size()-p.offset ||
                total>MaxFrameBytes-4 || p.size>MaxFrameBytes-total-4)
                return fail("proof packet bounds exceeded");
            total+=4+p.size;
        }
        std::vector<std::uint8_t> result{'P','Y','R','W',1,std::uint8_t(count>>8),std::uint8_t(count),0};
        result.reserve(total);
        for (const auto& p : packets) {
            for (int shift : {24,16,8,0}) result.push_back(std::uint8_t(p.size>>shift));
            result.insert(result.end(),bitstream.begin()+p.offset,bitstream.begin()+p.offset+p.size);
        }
        container=std::move(result);
        return true;
    } catch (const std::bad_alloc&) { return fail("proof fixture allocation failed"); }
}
}
