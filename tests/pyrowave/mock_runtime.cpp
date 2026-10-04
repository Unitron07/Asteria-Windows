// GPU-free C API test double. Never staged with an experimental owner package.
#define PYROWAVE_EXPORT_SYMBOLS
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan_core.h>
#include <pyrowave/pyrowave.h>
#include <cstring>

struct pyrowave_device_opaque {};
struct pyrowave_decoder_opaque { bool fragment; bool ready = false; };
static pyrowave_device_opaque device;
static bool preferFragment = false, emptyReport = false;
static bool createdFragment = false, decodedFragment = false;
static uint32_t decoderCreations = 0;
static uint32_t vendor = 0;
extern "C" __declspec(dllexport) void mock_configure(bool fragment, uint32_t vid, bool empty) {
    preferFragment = fragment; vendor = vid; emptyReport = empty;
}
extern "C" __declspec(dllexport) void mock_decoder_state(uint32_t* count, bool* fragment) {
    *count=decoderCreations; *fragment=createdFragment;
}
void pyrowave_get_api_version(uint32_t* a,uint32_t* b,uint32_t* c) { *a=0; *b=6; *c=0; }
pyrowave_result pyrowave_create_default_device(pyrowave_device* out) { *out=&device; return PYROWAVE_SUCCESS; }
void pyrowave_device_destroy(pyrowave_device) {}
void pyrowave_device_get_vk_device_handles(pyrowave_device,VkInstance* instance,VkPhysicalDevice* physical,VkDevice* logical) {
    if (instance) *instance=reinterpret_cast<VkInstance>(&device);
    if (physical) *physical=reinterpret_cast<VkPhysicalDevice>(&device);
    if (logical) *logical=reinterpret_cast<VkDevice>(&device);
}
static VKAPI_ATTR void VKAPI_CALL properties(VkPhysicalDevice,VkPhysicalDeviceProperties* out) {
    *out={}; out->vendorID=vendor; std::strcpy(out->deviceName,"Mock adapter");
}
extern "C" __declspec(dllexport) VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance,const char* name) {
    return std::strcmp(name,"vkGetPhysicalDeviceProperties")==0 ? reinterpret_cast<PFN_vkVoidFunction>(properties) : nullptr;
}
bool pyrowave_decoder_device_prefers_fragment_path(pyrowave_device) { return preferFragment; }
pyrowave_result pyrowave_decoder_create(const pyrowave_decoder_create_info* info,pyrowave_decoder* out) {
    // Expose the actual create-info and creation count to policy tests.
    createdFragment=info->fragment_path; ++decoderCreations;
    *out=new pyrowave_decoder_opaque{info->fragment_path}; return PYROWAVE_SUCCESS;
}
void pyrowave_decoder_destroy(pyrowave_decoder decoder) { delete decoder; }
void pyrowave_decoder_clear(pyrowave_decoder decoder) { decoder->ready=false; }
pyrowave_result pyrowave_decoder_push_packet(pyrowave_decoder decoder,const void*,size_t) { decoder->ready=true; return PYROWAVE_SUCCESS; }
bool pyrowave_decoder_decode_is_ready(pyrowave_decoder decoder,bool partial) { return decoder->ready && !partial; }
pyrowave_result pyrowave_decoder_decode_cpu_buffer_synchronous(pyrowave_decoder decoder,const pyrowave_cpu_buffer* out) {
    if (!decoder->ready) return PYROWAVE_ERROR_INVALID_ARGUMENT;
    decodedFragment=decoder->fragment;
    for (int p=0;p<3;++p) std::memset(out->data[p],p ? 128 : 16,out->plane_size_in_bytes[p]);
    return PYROWAVE_SUCCESS;
}
pyrowave_result pyrowave_decoder_decode_gpu_buffer(pyrowave_decoder decoder,const pyrowave_gpu_sync_operation*,
    const pyrowave_gpu_sync_operation*,const pyrowave_gpu_buffers*) {
    if (!decoder->ready) return PYROWAVE_ERROR_INVALID_ARGUMENT;
    decodedFragment=decoder->fragment;
    return PYROWAVE_SUCCESS;
}
void pyrowave_device_report_performance_stats(pyrowave_device,pyrowave_message_cb cb,void* userdata,bool reset) {
    if (emptyReport) return;
    cb(userdata,"Dequant: 0.125 ms per frame");
    cb(userdata,decodedFragment ? "iDWT fragment: 0.250 ms per frame" : "iDWT: 0.250 ms per frame");
    cb(userdata,reset ? "reset=true" : "reset=false");
}
