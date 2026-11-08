#include <VSL/lifetime/command_domain.hpp>
#include <VSL/sync/submission_completion.hpp>
#include <VSL/operation/submission_ops.hpp>
#include <VSL/lifetime/command_storage.hpp>
#include <VSL/lifetime/dependency_node.hpp>
#include <stdexcept>
#include <utility>

namespace VSL_NAMESPACE::lifetime {
    namespace {
        bool same_buffer(const std::shared_ptr<impl::CommandBufferImpl>& first, const std::shared_ptr<impl::CommandBufferImpl>& second);
        struct CommandMutation {
            CommandRecordingContext& context;
            explicit CommandMutation(CommandRecordingContext& context);
            ~CommandMutation() { context.mutating = false; }
        };
        bool same_buffer(const std::shared_ptr<impl::CommandBufferImpl>& first, const std::shared_ptr<impl::CommandBufferImpl>& second) {
            return first && first == second && !first.owner_before(second) && !second.owner_before(first);
        }
        CommandMutation::CommandMutation(CommandRecordingContext& context) : context(context) {
            if (context.closing || context.mutating || context.recording || context.returning_image || context.frame_buffer_recording)
                throw std::logic_error("Command ring cannot change during recording or mutation");
            context.mutating = true;
            ++context.generation;
        }
    } // namespace
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
                operation::reset_command_buffer(native->command_buffers.at(native->current_buffer_idx));
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
        presentation.clear();
        submitted.clear();
        recording.reset();
        if (native->recording_context == &context)
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
        if (!buffer._domain || !same_buffer(buffer._domain->buffer, buffer.data_) || buffer._domain->context.closing)
            throw std::logic_error("Command buffer has no live lifetime domain");
        return buffer._domain;
    }
    std::shared_ptr<CommandDomain> CommandAccess::Resolve(const std::shared_ptr<impl::CommandBufferImpl>& buffer) {
        auto domain = buffer && buffer->owner_context ? buffer->owner_context->domain.lock() : nullptr;
        if (!domain || !same_buffer(domain->buffer, buffer) || domain->context.closing)
            throw std::logic_error("Command buffer lifetime domain has expired");
        return domain;
    }
    std::shared_ptr<CommandDomain> CommandAccess::CreateDomain(std::shared_ptr<impl::CommandBufferImpl> buffer) {
        return std::make_shared<CommandDomain>(std::move(buffer));
    }
    CommandBuffer CommandAccess::Attach(std::shared_ptr<impl::CommandBufferImpl> buffer, Factory factory) {
        if (!buffer || !buffer.use_count() || !buffer->owner_node || !buffer->owner_node->matches(buffer) ||
            !buffer->owner_context || buffer->command_buffers.empty() || !factory)
            throw std::invalid_argument("Command ring attach requires its live canonical owner");
        if (auto domain = buffer->owner_context->domain.lock()) {
            if (!same_buffer(domain->buffer, buffer) || domain->context.closing)
                throw std::logic_error("Command ring has a different lifetime domain");
            return Buffer(domain);
        }
        if (buffer->recording_context)
            throw std::logic_error("Command ring is still retiring its previous domain");
        auto domain = factory(buffer);
        if (!domain || !same_buffer(domain->buffer, buffer))
            throw std::logic_error("Command domain factory did not preserve its canonical ring");
        if (buffer->recording_context || !buffer->owner_context->domain.expired())
            throw std::logic_error("Command ring was attached while creating its domain");
        buffer->owner_context->domain = domain;
        buffer->recording_context = &domain->context;
        return Buffer(domain);
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
        if (!data || !domain || !same_buffer(data->command_buffer, domain->buffer) || domain->context.closing)
            throw std::logic_error("Command manager requires the canonical buffer domain");
        CommandManager result(nullptr);
        result.data_ = std::move(data);
        result._domain = std::move(domain);
        return result;
    }
    void next_command_buffer(const CommandBuffer& buffer) {
        const auto domain = CommandAccess::Owner(buffer);
        const CommandMutation mutation(domain->context);
        const auto native = domain->buffer;
        if (native->command_buffers.empty() || native->current_buffer_idx >= native->command_buffers.size())
            throw std::logic_error("Command ring slot is invalid");
        native->current_buffer_idx = (native->current_buffer_idx + 1) % native->command_buffers.size();
    }
    void reset_command_buffers(const CommandBuffer& buffer, std::optional<std::uint32_t> index) {
        const auto domain = CommandAccess::Owner(buffer);
        const CommandMutation mutation(domain->context);
        const auto native = domain->buffer;
        if (index && *index >= native->command_buffers.size())
            throw std::out_of_range("Command buffer reset index");
        const auto submitted = domain->submissions;
        const size_t first = index.value_or(0), end = index ? size_t(*index) + 1 : native->command_buffers.size();
        for (size_t i = first; i < end; ++i)
            if (const auto& work = submitted.at(i); work && Completion(work->result).status() == Completion::Status::SUBMITTED)
                throw std::logic_error("Command buffer is still in GPU use");
        for (size_t i = first; i < end; ++i)
            operation::reset_command_buffer(native->command_buffers.at(i));
    }
} // namespace VSL_NAMESPACE::lifetime
