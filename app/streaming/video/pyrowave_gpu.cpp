#include "pyrowave_gpu.h"
#include "pyrowave_queue.h"
#include "pyrowave_sdl.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11_4.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <cstring>

namespace PyroWave {
using Microsoft::WRL::ComPtr;
struct GpuPresentation::Impl {
    Runtime& runtime;
    explicit Impl(Runtime& r) : runtime(r) {}
    std::string error;
    ComPtr<ID3D11Device5> device;
    ComPtr<ID3D11DeviceContext4> context;
    struct Slot {
        ComPtr<ID3D11Texture2D> planes[3];
        ComPtr<ID3D11Fence> fence;
        pyrowave_image images[3] = {};
        pyrowave_sync_object sync = nullptr;
        pyrowave_gpu_buffers buffers{};
        SDL_Texture* textures[2] = {};
        YuvRange range = YuvRange::Limited;
        uint64_t value = 0;
    } slots[FrameSlots::Count];
#define GPU_API(member, name) decltype(&name) member = nullptr
    GPU_API(imageCreate,pyrowave_image_create);
    GPU_API(imageDestroy,pyrowave_image_destroy);
    GPU_API(imageView,pyrowave_image_get_image_view);
    GPU_API(syncCreate,pyrowave_sync_object_create);
    GPU_API(syncDestroy,pyrowave_sync_object_destroy);
    GPU_API(syncSemaphore,pyrowave_sync_object_get_semaphore);
#undef GPU_API
    bool fail(const std::string& reason) { error=reason; return false; }
    bool hr(HRESULT result,const char* op) {
        return SUCCEEDED(result) || fail(std::string(op)+" HRESULT="+std::to_string(uint32_t(result)));
    }
    bool pyro(pyrowave_result result,const char* op) {
        return result==PYROWAVE_SUCCESS || fail(std::string(op)+" result="+std::to_string(result) +
            (result==PYROWAVE_ERROR_UNSUPPORTED_EXTERNAL_HANDLE ? " (PYROWAVE_ERROR_UNSUPPORTED_EXTERNAL_HANDLE)" : ""));
    }
};
GpuPresentation::GpuPresentation(Runtime& runtime) : m_Impl(new Impl(runtime)) {}
const std::string& GpuPresentation::error() const { return m_Impl->error; }
GpuPresentation::~GpuPresentation() {
    auto& p=*m_Impl;
    // Caller resets the codec decoder first (its destruction drains Vulkan).
    // Drain only our D3D fence payloads on teardown, never the device per frame.
    if (p.context) p.context->Flush();
    for (auto& slot:p.slots) {
        if (slot.fence && slot.value && slot.fence->GetCompletedValue()<slot.value) {
            HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
            if (event) {
                if (SUCCEEDED(slot.fence->SetEventOnCompletion(slot.value,event)))
                    WaitForSingleObject(event,5000);
                CloseHandle(event);
            }
        }
        for (auto texture:slot.textures) if (texture) SDL_DestroyTexture(texture);
        for (auto image:slot.images) if (image) p.imageDestroy(image);
        if (slot.sync) p.syncDestroy(slot.sync);
    }
}
bool GpuPresentation::initialize(SDL_Renderer* renderer,int width,int height) {
    auto& p=*m_Impl; auto& r=p.runtime;
    if (!r.m_Device || !r.m_Decoder || !r.m_Vulkan) return p.fail("Vulkan decoder/device not initialized");
    if (r.m_Width!=width || r.m_Height!=height) return p.fail("GPU presentation extent must match decoder");
#define RESOLVE(member, name) p.member=reinterpret_cast<decltype(p.member)>(GetProcAddress(static_cast<HMODULE>(r.m_Module),#name)); if (!p.member) return p.fail("missing GPU API: " #name)
    RESOLVE(imageCreate,pyrowave_image_create);
    RESOLVE(imageDestroy,pyrowave_image_destroy);
    RESOLVE(imageView,pyrowave_image_get_image_view);
    RESOLVE(syncCreate,pyrowave_sync_object_create);
    RESOLVE(syncDestroy,pyrowave_sync_object_destroy);
    RESOLVE(syncSemaphore,pyrowave_sync_object_get_semaphore);
#undef RESOLVE
    if (!r.m_Api.decodeGpu)
        return p.fail("missing GPU decode API");
    auto base=static_cast<ID3D11Device*>(rendererD3D11Device(renderer));
    if (!base) return p.fail("SDL renderer does not expose a D3D11 device");
    if (!p.hr(base->QueryInterface(IID_PPV_ARGS(&p.device)),"D3D11 device5")) return false;
    ComPtr<ID3D11DeviceContext> context;
    base->GetImmediateContext(&context);
    if (!p.hr(context.As(&p.context),"D3D11 context4")) return false;
    ComPtr<IDXGIDevice> dxgi; ComPtr<IDXGIAdapter> adapter;
    DXGI_ADAPTER_DESC desc{};
    if (!p.hr(base->QueryInterface(IID_PPV_ARGS(&dxgi)),"DXGI device") ||
        !p.hr(dxgi->GetAdapter(&adapter),"DXGI adapter") ||
        !p.hr(adapter->GetDesc(&desc),"DXGI adapter LUID")) return false;
    VkInstance instance{}; VkPhysicalDevice physical{};
    r.m_Api.deviceHandles(r.m_Device,&instance,&physical,nullptr);
    auto getProc=reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(static_cast<HMODULE>(r.m_Vulkan),"vkGetInstanceProcAddr"));
    auto props=reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(getProc(instance,"vkGetPhysicalDeviceProperties2"));
    if (!props) return p.fail("Vulkan adapter LUID query unavailable");
    VkPhysicalDeviceIDProperties ids{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};
    VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,&ids};
    props(physical,&properties);
    if (!ids.deviceLUIDValid || std::memcmp(ids.deviceLUID,&desc.AdapterLuid,VK_LUID_SIZE))
        return p.fail("Vulkan decoder and SDL D3D11 renderer adapters differ");
    // Preflight selected the device-recommended path for the GPU output probe.
    // No recreation is needed just to probe presentation capabilities.
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,"PyroWave GPU decoder path: %s",r.decoderPath());
    // D3D owns the allocations, as recommended by the pinned Windows interop test.
    // Each import and fence import is a real driver capability gate on x64/ARM64.
    for (auto& slot:p.slots) {
        if (!p.hr(p.device->CreateFence(0,D3D11_FENCE_FLAG_SHARED,IID_PPV_ARGS(&slot.fence)),"shared D3D11 fence")) return false;
        HANDLE handle=nullptr;
        if (!p.hr(slot.fence->CreateSharedHandle(nullptr,GENERIC_ALL,nullptr,&handle),"fence NT handle")) return false;
        pyrowave_sync_object_create_info sync{};
        sync.device=r.m_Device; sync.external_handle=reinterpret_cast<uintptr_t>(handle);
        sync.handle_type=VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT;
        sync.semaphore_type=VK_SEMAPHORE_TYPE_TIMELINE;
        if (!p.pyro(p.syncCreate(&sync,&slot.sync),"Vulkan D3D11 fence import")) {
            CloseHandle(handle); return false;
        } // Pinned API owns and closes a successfully imported NT handle.
        for (int plane=0;plane<3;++plane) {
            D3D11_TEXTURE2D_DESC td{};
            td.Width=width>>(plane!=0); td.Height=height>>(plane!=0);
            td.MipLevels=td.ArraySize=td.SampleDesc.Count=1;
            td.Format=DXGI_FORMAT_R8_UNORM; td.Usage=D3D11_USAGE_DEFAULT;
            td.BindFlags=D3D11_BIND_SHADER_RESOURCE | (r.m_FragmentPath ? D3D11_BIND_RENDER_TARGET : D3D11_BIND_UNORDERED_ACCESS);
            td.MiscFlags=D3D11_RESOURCE_MISC_SHARED|D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
            if (!p.hr(p.device->CreateTexture2D(&td,nullptr,&slot.planes[plane]),"shared R8 plane")) return false;
            ComPtr<IDXGIResource1> resource;
            if (!p.hr(slot.planes[plane].As(&resource),"shared plane resource") ||
                !p.hr(resource->CreateSharedHandle(nullptr,GENERIC_ALL,nullptr,&handle),"plane NT handle")) return false;
            VkImageCreateInfo image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            image.imageType=VK_IMAGE_TYPE_2D; image.format=VK_FORMAT_R8_UNORM;
            image.extent={td.Width,td.Height,1}; image.mipLevels=image.arrayLayers=1;
            image.samples=VK_SAMPLE_COUNT_1_BIT; image.tiling=VK_IMAGE_TILING_OPTIMAL;
            image.usage=VK_IMAGE_USAGE_SAMPLED_BIT | (r.m_FragmentPath ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : VK_IMAGE_USAGE_STORAGE_BIT);
            image.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
            pyrowave_image_create_info info{};
            info.device=r.m_Device; info.external_handle=reinterpret_cast<uintptr_t>(handle);
            info.handle_type=VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT;
            info.image_create_info=&image;
            if (!p.pyro(p.imageCreate(&info,&slot.images[plane]),"Vulkan shared R8 import")) {
                CloseHandle(handle); return false;
            }
            if (!p.pyro(p.imageView(slot.images[plane],VK_IMAGE_ASPECT_COLOR_BIT,VK_IMAGE_USAGE_STORAGE_BIT,&slot.buffers.planes[plane]),"R8 decode view")) return false;
        }
        void* planes[]={slot.planes[0].Get(),slot.planes[1].Get(),slot.planes[2].Get()};
        for (int full=0;full<2;++full) {
            slot.textures[full]=createLiveTexture(renderer,width,height,full ? YuvRange::Full : YuvRange::Limited,planes);
            if (!slot.textures[full]) return p.fail(std::string("SDL shared I420 wrapper: ")+SDL_GetError());
        }
    }
    return true;
}
bool GpuPresentation::decode(int index,const std::vector<uint8_t>& bytes,std::size_t& packets,DecodeTiming& timing) {
    auto& p=*m_Impl; auto& slot=p.slots[index];
    pyrowave_gpu_external_reference acquireImages[3],releaseImages[3];
    for (int i=0;i<3;++i) {
        // Discard old contents using the pin's documented UNDEFINED transition.
        acquireImages[i]={slot.images[i],VK_QUEUE_FAMILY_IGNORED};
        releaseImages[i]={slot.images[i],VK_QUEUE_FAMILY_EXTERNAL};
    }
    const auto semaphore=p.syncSemaphore(slot.sync);
    pyrowave_gpu_sync_operation acquire{acquireImages,3,{slot.value ? semaphore : VK_NULL_HANDLE,slot.value}};
    pyrowave_gpu_sync_operation release{releaseImages,3,{semaphore,slot.value+1}};
    Pixels unused; packets=0;
    if (!p.runtime.decodeImpl(bytes,unused,true,&packets,&timing,&slot.buffers,&acquire,&release)) return false;
    ++slot.value;
    slot.range=*p.runtime.liveRange();
    return true;
}
SDL_Texture* GpuPresentation::texture(int index) {
    auto& slot=m_Impl->slots[index];
    return slot.textures[slot.range==YuvRange::Full];
}
bool GpuPresentation::beginRender(int index) {
    auto& p=*m_Impl; auto& slot=p.slots[index];
    return p.hr(p.context->Wait(slot.fence.Get(),slot.value),"D3D11 decode fence wait");
}
bool GpuPresentation::endRender(int index) {
    auto& p=*m_Impl; auto& slot=p.slots[index];
    if (!p.hr(p.context->Signal(slot.fence.Get(),slot.value+1),"D3D11 reuse fence signal")) return false;
    ++slot.value;
    // Submit the signal even when SDL has no subsequent Present (minimized).
    // Flush submits work; it does not wait for GPU completion.
    p.context->Flush();
    return true;
}
}
