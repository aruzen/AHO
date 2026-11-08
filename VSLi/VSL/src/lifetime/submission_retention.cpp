#include <VSL/lifetime/command_domain.hpp>
#include <VSL/lifetime/submission_retention.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/sync/submission_completion.hpp>
#include <VSL/sync/resource_use_state.hpp>
#include <VSL/lifetime/descriptor_bindings.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <algorithm>
#include <stdexcept>

namespace VSL_NAMESPACE::lifetime {
    namespace {
        struct DescriptorRecording {
            impl::GraphicResourceImpl& resource;
            explicit DescriptorRecording(impl::GraphicResourceImpl& resource);
            ~DescriptorRecording() { resource.recording = false; }
        };
        bool contains_owner(const std::vector<std::shared_ptr<void>>& resources, const std::shared_ptr<void>& resource);
        DescriptorRecording::DescriptorRecording(impl::GraphicResourceImpl& resource) : resource(resource) {
            require_descriptor_valid(resource);
            if (resource.recording)
                throw std::logic_error("Descriptor resource is being recorded");
            if (resource.owner_context && resource.owner_context->node.updating())
                throw std::logic_error("Ownership root is being updated");
            resource.recording = true;
        }
        bool contains_owner(const std::vector<std::shared_ptr<void>>& resources, const std::shared_ptr<void>& resource) {
            return std::ranges::any_of(resources, [&](const auto& existing) {
                return !existing.owner_before(resource) && !resource.owner_before(existing);
            });
        }
    } // namespace
    void retain_resource(std::vector<std::shared_ptr<void>>& resources, const std::shared_ptr<void>& resource) {
        if (resource.use_count() && !contains_owner(resources, resource))
            resources.push_back(resource);
    }
    void retain_resource(std::vector<std::shared_ptr<void>>& resources, std::shared_ptr<void>&& resource) {
        auto accepted = std::move(resource);
        if (accepted.use_count() && !contains_owner(resources, accepted))
            resources.push_back(std::move(accepted));
    }
    void require_recording(const CommandBuffer& buffer, const Completion& completion) {
        if (!buffer.data_ || !buffer.data_->recording_context || !buffer.data_->recording_context->work || !completion.data_ ||
            buffer.data_->recording_context->work->result != completion.data_ || completion.data_->status != Completion::Status::PENDING)
            throw std::logic_error("Recording phase changed while tracking resources");
    }
    void record_descriptor_resources(impl::GraphicResourceImpl& resource, const CommandBuffer& buffer, ResourceRetention retention) {
        const DescriptorRecording recording(resource);
        const auto completion = buffer.completion();
        resource.use.mark_used(completion);
        for (const auto& [binding, elements] : resource.bindings)
            for (const auto& element : elements) {
                for (const auto& owner : element.owners)
                    if (retention == ResourceRetention::COMPLETION)
                        buffer.retain_until_complete(detail::OwnerAccess::Shared(owner));
                    else
                        buffer.retain(detail::OwnerAccess::Shared(owner));
                for (const auto* state : element.states)
                    state->mark_used(completion);
                for (const auto& owner : element.constraint.owners)
                    if (retention == ResourceRetention::COMPLETION)
                        buffer.retain_until_complete(detail::OwnerAccess::Shared(owner));
                    else
                        buffer.retain(detail::OwnerAccess::Shared(owner));
                for (const auto* state : element.constraint.states)
                    state->mark_used(completion);
            }
        require_recording(buffer, completion);
        require_descriptor_valid(resource);
    }
    void release_completed_resources(impl::SubmissionWork& submission) noexcept {
        submission.release_resources = nullptr;
        submission.completion_resources.clear();
    }
} // namespace VSL_NAMESPACE::lifetime

namespace VSL_NAMESPACE {
    void Completion::retain(const std::shared_ptr<void>& resource) const {
        auto work = data_ ? data_->work.lock() : nullptr;
        if (!work || data_->status != Status::PENDING)
            throw std::logic_error("Resources must be retained before submission");
        lifetime::retain_resource(work->resources, resource);
    }
    void Completion::retain(std::shared_ptr<void>&& resource) const {
        auto accepted = std::move(resource);
        auto work = data_ ? data_->work.lock() : nullptr;
        if (!work || data_->status != Status::PENDING)
            throw std::logic_error("Resources must be retained before submission");
        lifetime::retain_resource(work->resources, std::move(accepted));
    }
    void CommandBuffer::retain(const std::shared_ptr<void>& resource) const {
        if (!data_ || !data_->recording_context)
            throw std::invalid_argument("CommandBuffer is empty");
        if (resource.use_count() && data_->recording_context->work) {
            if (data_->recording_context->work->result->status != Completion::Status::PENDING)
                throw std::logic_error("Resources must be retained before submission");
            lifetime::retain_resource(data_->recording_context->work->resources, resource);
        }
    }
    void CommandBuffer::retain(std::shared_ptr<void>&& resource) const {
        auto accepted = std::move(resource);
        if (!data_ || !data_->recording_context)
            throw std::invalid_argument("CommandBuffer is empty");
        if (accepted.use_count() && data_->recording_context->work) {
            if (data_->recording_context->work->result->status != Completion::Status::PENDING)
                throw std::logic_error("Resources must be retained before submission");
            lifetime::retain_resource(data_->recording_context->work->resources, std::move(accepted));
        }
    }
    void CommandBuffer::retain_until_complete(const std::shared_ptr<void>& resource) const {
        if (!data_ || !data_->recording_context || !data_->recording_context->work || data_->recording_context->work->result->status != Completion::Status::PENDING)
            throw std::logic_error("Buffer retention requires a recording phase");
        if (resource.use_count()) {
            data_->recording_context->work->release_resources = lifetime::release_completed_resources;
            lifetime::retain_resource(data_->recording_context->work->completion_resources, resource);
        }
    }
    void CommandBuffer::retain_until_complete(std::shared_ptr<void>&& resource) const {
        auto accepted = std::move(resource);
        if (!data_ || !data_->recording_context || !data_->recording_context->work || data_->recording_context->work->result->status != Completion::Status::PENDING)
            throw std::logic_error("Buffer retention requires a recording phase");
        if (accepted.use_count()) {
            data_->recording_context->work->release_resources = lifetime::release_completed_resources;
            lifetime::retain_resource(data_->recording_context->work->completion_resources, std::move(accepted));
        }
    }
    impl::SubmissionWork::~SubmissionWork() {
        sync::finish_submission(*this);
    }
} // namespace VSL_NAMESPACE
