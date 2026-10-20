#ifdef AHO_CONTRACT_SCREENSHOT_PUBLIC_VSL
// スクリーンショットの実装がVulkan固有の識別子を直接使用しないことを確かめる。
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC poison vk VkDevice VkBuffer VkFence VkSemaphore VK_WHOLE_SIZE
#endif
#include "../../src/x/screenshot.cpp"
#endif
