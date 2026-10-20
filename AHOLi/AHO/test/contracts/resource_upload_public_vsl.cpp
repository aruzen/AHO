#ifdef AHO_CONTRACT_RESOURCE_UPLOAD_PUBLIC_VSL
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC poison vk VkDevice VkBuffer VkFence VkSemaphore VK_WHOLE_SIZE
#endif
#include "../../src/x/resource/buffer.cpp"
#include "../../src/x/resource/texture.cpp"
#endif
