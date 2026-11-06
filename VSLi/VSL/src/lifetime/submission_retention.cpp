#include <VSL/lifetime/submission_retention.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/sync/submission_completion.hpp>
#include <VSL/sync/resource_use_state.hpp>
#include <algorithm>
#include <stdexcept>

namespace VSL_NAMESPACE::lifetime {
    namespace {
        bool contains_owner(const std::vector<std::shared_ptr<void>>& resources, const std::shared_ptr<void>& resource);
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
    void record_descriptor_buffers(const impl::GraphicResourceImpl& resource, const CommandBuffer& buffer, ResourceRetention retention) {
        const auto completion = buffer.completion();
        for (const auto& [binding, refs] : resource.buffer_refs)
            for (const auto& ref : refs) {
                const auto& owner = resource.bindings.at(binding).at(ref.owner_index);
                if (retention == ResourceRetention::COMPLETION)
                    buffer.retain_until_complete(owner);
                else
                    buffer.retain(owner);
                ref.buffer->use->mark_used(completion);
            }
    }
    void release_completed_resources(impl::SubmissionImpl& submission) noexcept {
        submission.release_resources = nullptr;
        submission.completion_resources.clear();
    }
} // namespace VSL_NAMESPACE::lifetime

namespace VSL_NAMESPACE {
    void Completion::retain(const std::shared_ptr<void>& resource) const {
        if (!data_ || data_->status != Status::PENDING)
            throw std::logic_error("Resources must be retained before submission");
        lifetime::retain_resource(data_->resources, resource);
    }
    void Completion::retain(std::shared_ptr<void>&& resource) const {
        auto accepted = std::move(resource);
        if (!data_ || data_->status != Status::PENDING)
            throw std::logic_error("Resources must be retained before submission");
        lifetime::retain_resource(data_->resources, std::move(accepted));
    }
    void CommandBuffer::retain(const std::shared_ptr<void>& resource) const {
        if (!data_)
            throw std::invalid_argument("CommandBuffer is empty");
        if (resource.use_count() && data_->recording_submission)
            Completion{data_->recording_submission}.retain(resource);
    }
    void CommandBuffer::retain(std::shared_ptr<void>&& resource) const {
        auto accepted = std::move(resource);
        if (!data_)
            throw std::invalid_argument("CommandBuffer is empty");
        if (accepted.use_count() && data_->recording_submission)
            Completion{data_->recording_submission}.retain(std::move(accepted));
    }
    void CommandBuffer::retain_until_complete(const std::shared_ptr<void>& resource) const {
        if (!data_ || !data_->recording_submission || data_->recording_submission->status != Completion::Status::PENDING)
            throw std::logic_error("Buffer retention requires a recording phase");
        if (resource.use_count()) {
            data_->recording_submission->release_resources = lifetime::release_completed_resources;
            lifetime::retain_resource(data_->recording_submission->completion_resources, resource);
        }
    }
    void CommandBuffer::retain_until_complete(std::shared_ptr<void>&& resource) const {
        auto accepted = std::move(resource);
        if (!data_ || !data_->recording_submission || data_->recording_submission->status != Completion::Status::PENDING)
            throw std::logic_error("Buffer retention requires a recording phase");
        if (accepted.use_count()) {
            data_->recording_submission->release_resources = lifetime::release_completed_resources;
            lifetime::retain_resource(data_->recording_submission->completion_resources, std::move(accepted));
        }
    }
    impl::SubmissionImpl::~SubmissionImpl() {
        sync::finish_submission(*this);
    }
} // namespace VSL_NAMESPACE
