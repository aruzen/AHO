#include <VSL/utils/typed_frame_resources.hpp>
#include <VSL/vulkan/_pimpls.hpp>

namespace VSL_NAMESPACE::typed::detail {
    FrameResourceDomain::FrameResourceDomain(CommandManager manager) : manager(std::move(manager)), buffer(nullptr), count(0) {
        this->manager.get_device();
        buffer = this->manager.get_buffer();
        if (!buffer.data_ || !buffer.get_size())
            throw std::invalid_argument("Typed frame resources require command buffer slots");
        count = buffer.get_size();
    }

    FrameResourceAccess FrameResourceDomain::access(const ComputePhase& phase) const {
        if (!manager.data_ || phase.manager.data_ != manager.data_ || manager.data_->command_buffer != buffer.data_)
            throw std::invalid_argument("Typed frame resources belong to a different command manager or buffer ring");
        const auto completion = phase.completion();
        if (phase.state != ComputePhase::State::WAIT_SUBMIT || !completion ||
            completion.status() != Completion::Status::PENDING || !buffer.data_->recording ||
            buffer.data_->recording_submission != completion.data_ ||
            phase.frame_slot != buffer.data_->current_buffer_idx ||
            phase.frame_slot >= count || buffer.data_->command_buffers.size() != count)
            throw std::logic_error("Typed frame resources require the currently recording phase");
        return {phase.frame_slot, completion};
    }
} // namespace VSL_NAMESPACE::typed::detail
