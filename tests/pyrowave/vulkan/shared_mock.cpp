#define PYROWAVE_EXPORT_SYMBOLS
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan_core.h>
#include <pyrowave/pyrowave.h>
#include <windows.h>
#include <cstring>
namespace {
    pyrowave_device_create_info borrowed{};
    int mode=0, defaultCalls=0, gpuCalls=0, cpuCalls=0, destroyed=0;
    bool fragment=false;
}
extern "C" {
__declspec(dllexport) void mock_configure(int m,bool f) { mode=m; fragment=f; defaultCalls=gpuCalls=cpuCalls=destroyed=0; }
__declspec(dllexport) void mock_counts(int* d,int* g,int* c,int* t) { *d=defaultCalls; *g=gpuCalls; *c=cpuCalls; *t=destroyed; }
void pyrowave_get_api_version(uint32_t* a,uint32_t* b,uint32_t* c) { *a=0;*b=6;*c=0; }
pyrowave_result pyrowave_create_default_device(pyrowave_device*) { ++defaultCalls; return PYROWAVE_ERROR_NO_VULKAN; }
#ifndef STAGE3_NO_BORROW
pyrowave_result pyrowave_create_device(const pyrowave_device_create_info* info,pyrowave_device* out) {
    if (mode==1) return PYROWAVE_ERROR_NO_VULKAN;
    borrowed=*info; *out=reinterpret_cast<pyrowave_device>(uintptr_t(10)); return PYROWAVE_SUCCESS;
}
#endif
void pyrowave_device_destroy(pyrowave_device) { ++destroyed; borrowed.queue_lock_callback(borrowed.userdata); borrowed.queue_unlock_callback(borrowed.userdata); }
void pyrowave_device_get_vk_device_handles(pyrowave_device,VkInstance* i,VkPhysicalDevice* p,VkDevice* d) {
    if(i) *i=mode==2 ? reinterpret_cast<VkInstance>(uintptr_t(11)) : borrowed.instance;
    if(p) *p=mode==3 ? reinterpret_cast<VkPhysicalDevice>(uintptr_t(12)) : borrowed.physical_device;
    if(d) *d=mode==4 ? reinterpret_cast<VkDevice>(uintptr_t(13)) : borrowed.device;
}
pyrowave_result pyrowave_device_set_queue_type(pyrowave_device,VkQueueFlagBits flags) { return flags==VK_QUEUE_GRAPHICS_BIT ? PYROWAVE_SUCCESS : PYROWAVE_ERROR_INVALID_ARGUMENT; }
bool pyrowave_decoder_device_prefers_fragment_path(pyrowave_device) { return fragment; }
pyrowave_result pyrowave_decoder_create(const pyrowave_decoder_create_info* info,pyrowave_decoder* out) {
    if (info->fragment_path && !fragment) return PYROWAVE_ERROR_INVALID_ARGUMENT;
    *out=reinterpret_cast<pyrowave_decoder>(uintptr_t(14)); return PYROWAVE_SUCCESS;
}
void pyrowave_decoder_destroy(pyrowave_decoder) { borrowed.queue_lock_callback(borrowed.userdata); borrowed.queue_unlock_callback(borrowed.userdata); }
void pyrowave_decoder_clear(pyrowave_decoder) {}
pyrowave_result pyrowave_decoder_push_packet(pyrowave_decoder,const void*,size_t) { return mode==5 ? PYROWAVE_ERROR_INVALID_ARGUMENT : PYROWAVE_SUCCESS; }
bool pyrowave_decoder_decode_is_ready(pyrowave_decoder,bool) { return true; }
pyrowave_result pyrowave_decoder_decode_cpu_buffer_synchronous(pyrowave_decoder,const pyrowave_cpu_buffer*) { ++cpuCalls; return PYROWAVE_ERROR_INVALID_ARGUMENT; }
pyrowave_result pyrowave_decoder_decode_gpu_buffer(pyrowave_decoder,const pyrowave_gpu_sync_operation* a,const pyrowave_gpu_sync_operation* r,const pyrowave_gpu_buffers*) {
    ++gpuCalls;
    if(a->num_images || r->num_images || !r->sync.semaphore || !r->sync.value) return PYROWAVE_ERROR_INVALID_ARGUMENT;
    borrowed.queue_lock_callback(borrowed.userdata); borrowed.queue_unlock_callback(borrowed.userdata);
    return mode==6 ? PYROWAVE_ERROR_INVALID_ARGUMENT : PYROWAVE_SUCCESS;
}
static void VKAPI_CALL properties(VkPhysicalDevice,VkPhysicalDeviceProperties* p) { *p={}; std::strcpy(p->deviceName,"mock shared device"); }
__declspec(dllexport) PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance,const char* name) {
    return std::strcmp(name,"vkGetPhysicalDeviceProperties")==0 ? reinterpret_cast<PFN_vkVoidFunction>(properties) : nullptr;
}
}
