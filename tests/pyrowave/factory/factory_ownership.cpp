#include <vulkan/vulkan_core.h>
#include <pyrowave.h>
#include <functional>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <stdexcept>
static void require(bool ok) { if(!ok) throw std::runtime_error("pinned factory ownership failure"); }
namespace Util { inline void set_thread_logging_interface(void*) {} }
static void* null_logger=nullptr;
struct InstanceFactory {
    virtual ~InstanceFactory()=default;
    virtual VkInstance create_instance(const VkInstanceCreateInfo*)=0;
    virtual const VkInstanceCreateInfo* get_existing_create_info()=0;
    virtual bool factory_owns_created_instance()=0;
};
struct DeviceFactory {
    virtual ~DeviceFactory()=default;
    virtual VkDevice create_device(VkPhysicalDevice,const VkDeviceCreateInfo*)=0;
    virtual const VkDeviceCreateInfo* get_existing_create_info()=0;
    virtual bool factory_owns_created_device()=0;
    virtual VkQueue get_queue(uint32_t,uint32_t)=0;
};
struct ContextOptions { bool memory_priorities=true,lean_memory_mode=false; };
struct Context {
    inline static int failure=0,instances=0,devices=0;
    InstanceFactory* instance=nullptr; DeviceFactory* device=nullptr;
    static bool init_loader(PFN_vkGetInstanceProcAddr) { return failure!=1; }
    void set_instance_factory(InstanceFactory* p) { instance=p; }
    void set_device_factory(DeviceFactory* p) { device=p; }
    bool init_instance(const char*const*,uint32_t) {
        ++instances; require(instance->factory_owns_created_instance());
        require(instance->get_existing_create_info()!=nullptr); return failure!=2;
    }
    bool init_device(VkPhysicalDevice,VkSurfaceKHR,const char*const*,uint32_t) {
        ++devices; require(device->factory_owns_created_device());
        require(device->get_existing_create_info()!=nullptr);
        require(device->get_queue(0,0)!=VK_NULL_HANDLE); return failure!=3;
    }
};
struct Device {
    std::function<void()> lock,unlock;
    void set_context(Context&,const ContextOptions& opts) { require(!opts.memory_priorities && opts.lean_memory_mode); }
    void set_queue_lock(std::function<void()> a,std::function<void()> b) { lock=a; unlock=b; }
};
struct pyrowave_device_opaque {
    inline static unsigned allocated=0,destroyed=0,live=0;
    Context context; Device device;
    pyrowave_device_opaque() { ++allocated; ++live; }
    ~pyrowave_device_opaque() { ++destroyed; --live; }
};
static std::mutex global_device_lock;
#include "pinned_factory.inc"
int main() {
    try {
        VkInstanceCreateInfo ici{}; VkDeviceCreateInfo dci{};
        pyrowave_device_create_queue_info queue{}; queue.queue=reinterpret_cast<VkQueue>(uintptr_t(4));
        pyrowave_device_create_info info{}; info.instance_create_info=&ici; info.device_create_info=&dci;
        info.queue_info=&queue; info.queue_info_count=1;
        for(int failure=1;failure<=3;++failure) {
            Context::failure=failure;
            auto before=pyrowave_device_opaque::allocated;
            for(unsigned n=0;n<10000;++n) {
                pyrowave_device out=nullptr;
                require(pyrowave_create_device(&info,&out)==(failure==1 ? PYROWAVE_ERROR_INVALID_ARGUMENT : PYROWAVE_ERROR_NO_VULKAN));
                require(!out && pyrowave_device_opaque::live==0);
                require(pyrowave_device_opaque::allocated==pyrowave_device_opaque::destroyed);
            }
            require(pyrowave_device_opaque::allocated-before==(failure==1 ? 0 : 10000));
        }
        Context::failure=0; unsigned callbacks=0;
        info.userdata=&callbacks; info.queue_lock_callback=[](void* p) { ++*static_cast<unsigned*>(p); };
        info.queue_unlock_callback=info.queue_lock_callback;
        for(unsigned n=0;n<10000;++n) {
            pyrowave_device out=nullptr; require(pyrowave_create_device(&info,&out)==PYROWAVE_SUCCESS && out);
            require(pyrowave_device_opaque::live==1); out->device.lock(); out->device.unlock();
            pyrowave_device_destroy(out); require(pyrowave_device_opaque::live==0);
        }
        require(callbacks==20000 && pyrowave_device_opaque::allocated==30000 && pyrowave_device_opaque::destroyed==30000);
        require(Context::instances==30000 && Context::devices==20000);
        std::cout<<"PASS: extracted pinned factory; 10000 loader, 10000 instance, 10000 device failures; unchanged errors/null output; 10000 successful create/destroy; allocations=destructions=30000 live=0\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
