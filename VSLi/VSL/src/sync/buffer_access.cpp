#include <VSL/sync/buffer_access.hpp>

namespace VSL_NAMESPACE::sync {
    BufferCopyVisibility buffer_copy_visibility(bool host_read) {
        auto stages = vk::PipelineStageFlags(vk::PipelineStageFlagBits::eAllCommands);
        auto access = vk::AccessFlags(vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite);
        if (host_read) {
            stages |= vk::PipelineStageFlagBits::eHost;
            access |= vk::AccessFlagBits::eHostRead;
        }
        return {vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eTransfer, stages,
                vk::AccessFlagBits::eMemoryWrite, vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite, access};
    }
} // namespace VSL_NAMESPACE::sync
