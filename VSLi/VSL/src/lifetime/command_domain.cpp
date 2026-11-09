#include <VSL/lifetime/command_domain.hpp>
#include <VSL/sync/submission_completion.hpp>
#include <VSL/sync/device_call.hpp>
#include <VSL/operation/submission_ops.hpp>
#include <VSL/lifetime/command_storage.hpp>
#include <VSL/lifetime/dependency_node.hpp>
#include <VSL/lifetime/presentation_retention.hpp>
#include <VSL/lifetime/submission_retention.hpp>
#include <VSL/lifetime/recovery_batch.hpp>
#include <VSL/lifetime/resource_arena.hpp>
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
            if (context.closing || context.mutating || context.recording || context.submitting || context.returning_image || context.frame_buffer_recording)
                throw std::logic_error("Command ring cannot change during recording or mutation");
            context.mutating = true;
            ++context.generation;
        }
    } // namespace
    CommandDomain::CommandDomain(std::shared_ptr<impl::CommandBufferImpl> buffer)
        : buffer(std::move(buffer)), submissions(this->buffer->command_buffers.size()) {
        inherit_quarantined_submissions(*this);
    }

    std::shared_ptr<impl::FenceImpl> acquire_fences(CommandDomain& domain, LogicalDeviceAccessor device) {
        if (domain.context.closing || domain.context.recording || domain.context.submitting || domain.context.returning_image ||
            domain.context.mutating || domain.context.frame_buffer_recording)
            throw std::logic_error("Command domain cannot prepare acquisition fences");
        if (!domain.acquire_fences) {
            const auto buffer = domain.buffer;
            const auto generation = domain.context.generation;
            const auto count = buffer->command_buffers.size();
            if (!count || count > std::numeric_limits<std::uint32_t>::max())
                throw std::invalid_argument("Acquisition fences require a nonzero uint32 frame count");
            static constexpr std::array<typed::ResourceRequest, 1> REQUESTS{{{{1}, storage::ResourceKind::FENCE}}};
            static constexpr typed::ResourcePlan<1> PLAN(REQUESTS);
            const std::array<ResourceInit, 1> inputs{FenceCreateInfo{count, false}};
            ResourceArena arena(device, PLAN, inputs);
            if (domain.acquire_fences || domain.buffer != buffer || domain.context.generation != generation ||
                domain.context.closing || domain.context.recording || domain.context.submitting || domain.context.returning_image || domain.context.mutating ||
                domain.context.frame_buffer_recording || buffer->command_buffers.size() != count)
                throw std::logic_error("Command domain changed while constructing acquisition fences");
            domain.acquire_fences = arena.fence({1}).data_;
        }
        return domain.acquire_fences;
    }

    CommandDomain::~CommandDomain() {
        context.closing = true;
        context.work = nullptr;
        const auto native = buffer;
        auto recording = std::move(recording_submission);
        auto submitted = std::move(submissions);
        auto presentation = std::move(presentation_domains);
        const bool active = native->command_pool && native->command_pool->device && sync::NativeDeviceCall::Active(native->command_pool->device->device);
        if (context.recording) {
            try {
                if (!active)
                    operation::reset_command_buffer(native->command_buffers.at(native->current_buffer_idx));
            } catch (...) {
            }
            context.recording = false;
        }
        const auto finish = [&](const std::shared_ptr<impl::SubmissionWork>& work) {
            if (!work)
                return;
            if (active && (work->result->native.active || sync::native_pending(*work->result) || work->result->status == Completion::Status::SUBMITTED)) {
                quarantine_submission(work, native);
                return;
            }
            sync::finish_submission(*work);
        };
        finish(recording);
        for (const auto& work : submitted)
            finish(work);
        presentation.clear();
        submitted.clear();
        recording.reset();
        if (native->recording_context == &context)
            native->recording_context = nullptr;
    }

    void CommandDomain::wait_idle() {
        if (context.closing || context.submitting || (recording_submission && recording_submission->result->native.active) ||
            std::ranges::any_of(submissions, [](const auto& work) { return work && (work->result->native.active || work->result->native.observing); }))
            throw std::logic_error("Command domain is closing");
        recover_command_domain(*this);
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
            if (const auto& work = submitted.at(i); work && Completion(work->result).busy())
                throw std::logic_error("Command buffer is still in GPU use");
        for (size_t i = first; i < end; ++i)
            operation::reset_command_buffer(native->command_buffers.at(i));
    }
} // namespace VSL_NAMESPACE::lifetime
