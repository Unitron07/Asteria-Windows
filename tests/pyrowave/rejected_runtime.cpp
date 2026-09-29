// Deliberately incomplete test DLLs; no Vulkan, codec or GPU calls.
#include <cstdint>
extern "C" __declspec(dllexport) void pyrowave_get_api_version(
    std::uint32_t* major, std::uint32_t* minor, std::uint32_t* patch) {
    *major=0; *minor=TEST_API_MINOR; *patch=0;
}
