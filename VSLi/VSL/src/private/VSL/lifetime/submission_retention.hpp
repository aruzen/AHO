#pragma once
#include <VSL/vulkan/pv.hpp>
#include <VSL/lifetime/resource_use.hpp>
#include <memory>
#include <vector>

namespace VSL_NAMESPACE {
    struct CommandBuffer;
}

namespace VSL_NAMESPACE::lifetime {
    // control block単位で保持する。同じarenaの別slotを指すaliasも一回だけ保持する。
    void retain_resource(std::vector<std::shared_ptr<void>>& resources, const std::shared_ptr<void>& resource);
    void retain_resource(std::vector<std::shared_ptr<void>>& resources, std::shared_ptr<void>&& resource);
    void record_descriptor_resources(const impl::GraphicResourceImpl& resource, const CommandBuffer& buffer, ResourceRetention retention = ResourceRetention::SUBMISSION);
    void release_completed_resources(impl::SubmissionImpl& submission) noexcept;
} // namespace VSL_NAMESPACE::lifetime
