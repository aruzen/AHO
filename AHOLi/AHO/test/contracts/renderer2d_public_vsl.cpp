#ifdef AHO_CONTRACT_RENDERER2D_PUBLIC_VSL
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC poison vk VkDevice VkBuffer VkFence VkSemaphore VK_WHOLE_SIZE
#endif
#include "../../src/x/drawer/renderer2d.cpp"
#include "../../src/x/command/draw2d.cpp"
#endif
