#pragma once
#include <VSL/vulkan/pv.hpp>
#include <VSL/lifetime/resource_use.hpp>
#include <VSL/sync/device_call.hpp>
#include <memory>
#include <vector>
#include <span>

namespace VSL_NAMESPACE {
    struct CommandBuffer;
}

namespace VSL_NAMESPACE::lifetime {
    struct SurfacePresentationDomain;
    struct CommandDomain;
    std::shared_ptr<void> pin_submission_owner(const impl::SubmissionWork& work) noexcept;
    class SubmissionObservation {
        std::shared_ptr<void> _owner;
        impl::SubmissionImpl* _result = nullptr;

    public:
        explicit SubmissionObservation(impl::SubmissionWork& work) noexcept;
        ~SubmissionObservation();
        SubmissionObservation(const SubmissionObservation&) = delete;
        SubmissionObservation& operator=(const SubmissionObservation&) = delete;
        SubmissionObservation(SubmissionObservation&& other) noexcept;
        bool active() const noexcept { return _result != nullptr; }
        bool owns(const impl::SubmissionImpl& result) const noexcept { return _result == &result; }
        void finish() noexcept;
    };
    class SubmissionTransaction {
        CommandDomain& _domain;
        impl::SubmissionWork& _work;
        bool _owns_context, _active = true;
        sync::NativeDeviceCall _native_call;

    public:
        SubmissionTransaction(CommandDomain& domain, impl::SubmissionWork& work);
        ~SubmissionTransaction();
        SubmissionTransaction(const SubmissionTransaction&) = delete;
        SubmissionTransaction& operator=(const SubmissionTransaction&) = delete;
        void finish() noexcept;
    };
    void abort_recording(CommandDomain& domain, impl::SubmissionWork& work, std::uint32_t slot) noexcept;
    void detach_recording_cpu(CommandDomain& domain, impl::SubmissionWork& work) noexcept;
    void fail_recording(CommandDomain& domain, impl::SubmissionWork& work, std::uint32_t slot,
                        Completion::Failure failure, std::int32_t error_code) noexcept;
    class SubmissionSnapshot {
        std::shared_ptr<impl::LogicalDeviceImpl> _device;
        std::shared_ptr<impl::SurfaceImpl> _surface;
        std::shared_ptr<impl::SubmissionWork> _selected, _foreign, _ready;
        std::vector<SubmissionObservation> _observations;
        bool _collecting = false, _pending = false;
        void capture(const std::shared_ptr<impl::CommandBufferImpl>& ring,
                     std::span<const std::shared_ptr<SurfacePresentationDomain>> presentations);

    public:
        explicit SubmissionSnapshot(const std::shared_ptr<impl::CommandBufferImpl>& ring,
                                    std::span<const std::shared_ptr<SurfacePresentationDomain>> presentations = {});
        SubmissionSnapshot(std::shared_ptr<impl::LogicalDeviceImpl> device, std::shared_ptr<impl::SurfaceImpl> surface);
        ~SubmissionSnapshot();
        SubmissionSnapshot(const SubmissionSnapshot&) = delete;
        SubmissionSnapshot& operator=(const SubmissionSnapshot&) = delete;
        void save_observations(std::int32_t idle_result) noexcept;
        void bookkeep() noexcept;
        void release() noexcept;
        bool pending() const noexcept { return _pending; }
        bool captured() const noexcept { return bool(_selected); }
        void finish() noexcept;
    };
    void quarantine_submission(const std::shared_ptr<impl::SubmissionWork>& work, const std::shared_ptr<impl::CommandBufferImpl>& buffer) noexcept;
    std::shared_ptr<impl::SubmissionWork> unquarantine_submission(impl::SubmissionWork& work) noexcept;
    size_t quarantined_submission_count() noexcept;
    void inherit_quarantined_submissions(CommandDomain& domain);
    // control block単位で保持する。同じarenaの別slotを指すaliasも一回だけ保持する。
    void retain_resource(std::vector<std::shared_ptr<void>>& resources, const std::shared_ptr<void>& resource);
    void retain_resource(std::vector<std::shared_ptr<void>>& resources, std::shared_ptr<void>&& resource);
    void retain_until_complete(impl::SubmissionWork& submission, std::shared_ptr<void> resource);
    void require_recording(const CommandBuffer& buffer, const Completion& completion);
    void record_resource_state(const ResourceUseState& state, const CommandBuffer& buffer, const Completion& completion);
    void record_descriptor_resources(impl::GraphicResourceImpl& resource, const CommandBuffer& buffer, ResourceRetention retention = ResourceRetention::SUBMISSION);
    void release_completed_resources(impl::SubmissionWork& submission) noexcept;
} // namespace VSL_NAMESPACE::lifetime
