#ifdef AHO_CONTRACT_RESOURCE_UPLOAD_PUBLIC_VSL
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC poison vk VkDevice VkBuffer VkFence VkSemaphore VK_WHOLE_SIZE
#endif
#include "../../src/resource/buffer.cpp"
#include "../../src/command/buffer_upload.cpp"
#include "../../src/x/resource/texture.cpp"
#include "../../src/x/command/texture_update.cpp"
#endif
