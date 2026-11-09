#include <VSL/lifetime/command_domain.hpp>
#include <VSL/sync/submission_completion.hpp>
#include <VSL/operation/submission_ops.hpp>
#include <VSL/lifetime/command_storage.hpp>
#include <VSL/lifetime/dependency_node.hpp>
#include <VSL/lifetime/presentation_retention.hpp>
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
            if (context.closing || context.mutating || context.recording || context.returning_image || context.frame_buffer_recording)
                throw std::logic_error("Command ring cannot change during recording or mutation");
            context.mutating = true;
            ++context.generation;
        }
    } // namespace
    CommandDomain::CommandDomain(std::shared_ptr<impl::CommandBufferImpl> buffer)
        : buffer(std::move(buffer)), submissions(this->buffer->command_buffers.size()) {}

    std::shared_ptr<impl::FenceImpl> acquire_fences(CommandDomain& domain, LogicalDeviceAccessor device) {
        if (domain.context.closing || domain.context.recording || domain.context.returning_image ||
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
                domain.context.closing || domain.context.recording || domain.context.returning_image || domain.context.mutating ||
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
        std::vector<std::shared_ptr<impl::SubmissionWork>> submitted;
        for (const auto& work : submissions)
            if (work && work->result->status == Completion::Status::SUBMITTED)
                submitted.push_back(work);
        std::vector<PresentationSnapshot> presentation;
        presentation.reserve(presentation_domains.size());
        for (const auto& domain : presentation_domains)
            presentation.push_back(snapshot_presentations(domain, native->command_pool->device->device));
        const auto idle = operation::wait_device_idle(native->command_pool->device->device);
        if (idle != vk::Result::eSuccess && idle != vk::Result::eErrorDeviceLost)
            vk::detail::throwResultException(idle, "Device idle failed");
        if (idle == vk::Result::eErrorDeviceLost) {
            // 全snapshotへ損失を保存してからownerを外す。callbackからpeerのFenceを成功観測させない。
            for (const auto& work : submitted)
                sync::save_terminal(*work, Completion::Status::FAILED, Completion::Failure::DEVICE_LOST, static_cast<std::int32_t>(idle));
            for (const auto& snapshot : presentation)
                for (const auto& record : snapshot.records)
                    sync::fail_present(record->state, idle);
        }
        for (const auto& snapshot : presentation)
            collect_quarantined_presentations(snapshot.domain->surface, native->command_pool->device->device, idle);
        for (const auto& work : submitted)
            sync::finish_submission(*work);
        for (auto& snapshot : presentation)
            finish_presentations(snapshot, idle);
        if (idle == vk::Result::eErrorDeviceLost)
            vk::detail::throwResultException(idle, "Device lost while waiting idle");
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
