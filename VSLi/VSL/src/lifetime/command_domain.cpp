#include <VSL/lifetime/command_domain.hpp>
#include <VSL/sync/submission_completion.hpp>
#include <VSL/operation/submission_ops.hpp>
#include <stdexcept>
#include <utility>

namespace VSL_NAMESPACE::lifetime {
    CommandDomain::CommandDomain(std::shared_ptr<impl::CommandBufferImpl> buffer)
        : buffer(std::move(buffer)), submissions(this->buffer->command_buffers.size()) {}

    CommandDomain::~CommandDomain() {
        context.closing = true;
        context.work = nullptr;
        const auto native = buffer;
        auto recording = std::move(recording_submission);
        auto submitted = std::move(submissions);
        auto presentation = std::move(presentation_resources);
        if (context.recording) {
            try {
                native->command_buffers.at(native->current_buffer_idx).reset();
            } catch (...) {
            }
            context.recording = false;
        }
        if (recording)
            sync::finish_submission(*recording);
        for (const auto& work : submitted)
            if (work)
                sync::finish_submission(*work);
        if (!presentation.empty()) {
            auto result = operation::wait_device_idle(native->command_pool->device->device);
            while (result != vk::Result::eSuccess && result != vk::Result::eErrorDeviceLost)
                result = operation::wait_device_idle(native->command_pool->device->device);
        }
        native->recording_context = nullptr;
    }

    void CommandDomain::wait_idle() {
        if (context.closing)
            throw std::logic_error("Command domain is closing");
        const auto native = buffer;
        const auto submitted = submissions;
        native->command_pool->device->device.waitIdle();
        auto presentation = std::move(presentation_resources);
        for (const auto& work : submitted)
            if (work)
                sync::finish_submission(*work);
    }

    const std::shared_ptr<CommandDomain>& CommandAccess::Owner(const CommandBuffer& buffer) {
        if (!buffer._domain || buffer._domain->buffer != buffer.data_ || buffer._domain->context.closing)
            throw std::logic_error("Command buffer has no live lifetime domain");
        return buffer._domain;
    }
    std::shared_ptr<CommandDomain> CommandAccess::Resolve(const std::shared_ptr<impl::CommandBufferImpl>& buffer) {
        auto domain = buffer ? buffer->domain.lock() : nullptr;
        if (!domain || domain->buffer != buffer || domain->context.closing ||
            domain->buffer.owner_before(buffer) || buffer.owner_before(domain->buffer))
            throw std::logic_error("Command buffer lifetime domain has expired");
        return domain;
    }
    CommandBuffer CommandAccess::Buffer(const std::shared_ptr<CommandDomain>& domain) {
        if (!domain || domain->context.closing)
            throw std::logic_error("Command buffer lifetime domain has expired");
        CommandBuffer result(nullptr);
        result.data_ = domain->buffer;
        result._domain = domain;
        return result;
    }
    CommandManager CommandAccess::Manager(std::shared_ptr<impl::CommandManagerImpl> data, std::shared_ptr<CommandDomain> domain) {
        if (!data || !domain || data->command_buffer != domain->buffer || domain->context.closing)
            throw std::logic_error("Command manager requires the canonical buffer domain");
        CommandManager result(nullptr);
        result.data_ = std::move(data);
        result._domain = std::move(domain);
        return result;
    }
} // namespace VSL_NAMESPACE::lifetime
