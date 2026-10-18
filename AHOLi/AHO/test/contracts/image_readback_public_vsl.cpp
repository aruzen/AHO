#ifdef AHO_CONTRACT_READBACK_PUBLIC_VSL
// Compile the product implementation while rejecting native Vulkan identifiers.
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC poison vk VkDevice VkBuffer VkFence VkSemaphore VK_WHOLE_SIZE
#endif
#include "../../src/image_readback.cpp"
#endif
