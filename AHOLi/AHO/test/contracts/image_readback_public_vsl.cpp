#ifdef AHO_CONTRACT_READBACK_PUBLIC_VSL
// Vulkan固有の識別子を禁止して製品コードをコンパイルし、直接依存を検出する。
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC poison vk VkDevice VkBuffer VkFence VkSemaphore VK_WHOLE_SIZE
#endif
#include "../../src/image_readback.cpp"
#endif
