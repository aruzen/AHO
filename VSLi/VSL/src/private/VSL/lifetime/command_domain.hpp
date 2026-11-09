#pragma once
#include <VSL/vulkan/_pimpls.hpp>

namespace VSL_NAMESPACE::lifetime {
    struct SurfacePresentationDomain;
    // Domain内で住所が安定する。通常の記録は所有を探索せず解決済みWorkへ届く。
    struct CommandRecordingContext {
        impl::SubmissionWork* work = nullptr;
        std::optional<std::uint32_t> acquired_image_index;
        std::weak_ptr<impl::SwapchainImpl> acquired_swapchain;
        std::uint64_t generation = 0;
        bool recording = false;
        bool returning_image = false;
        bool frame_buffer_recording = false;
        bool closing = false;
        bool mutating = false;
        bool submitting = false;
    };
    struct CommandDomain {
        std::shared_ptr<impl::CommandBufferImpl> buffer;
        std::vector<std::shared_ptr<impl::SubmissionWork>> submissions;
        std::shared_ptr<impl::SubmissionWork> recording_submission;
        std::shared_ptr<impl::FenceImpl> acquire_fences;
        std::vector<std::shared_ptr<SurfacePresentationDomain>> presentation_domains;
        CommandRecordingContext context;

        explicit CommandDomain(std::shared_ptr<impl::CommandBufferImpl> buffer);
        CommandDomain(const CommandDomain&) = delete;
        CommandDomain& operator=(const CommandDomain&) = delete;
        ~CommandDomain();
        void wait_idle();
    };
    std::shared_ptr<impl::FenceImpl> acquire_fences(CommandDomain& domain, LogicalDeviceAccessor device);
    struct CommandAccess {
        static const std::shared_ptr<CommandDomain>& Owner(const CommandBuffer& buffer);
        static std::shared_ptr<CommandDomain> Resolve(const std::shared_ptr<impl::CommandBufferImpl>& buffer);
        using Factory = std::shared_ptr<CommandDomain> (*)(std::shared_ptr<impl::CommandBufferImpl>);
        static std::shared_ptr<CommandDomain> CreateDomain(std::shared_ptr<impl::CommandBufferImpl> buffer);
        static CommandBuffer Attach(std::shared_ptr<impl::CommandBufferImpl> buffer, Factory factory = CreateDomain);
        static CommandBuffer Buffer(const std::shared_ptr<CommandDomain>& domain);
        static CommandManager Manager(std::shared_ptr<impl::CommandManagerImpl> data, std::shared_ptr<CommandDomain> domain);
    };
    void reset_command_buffers(const CommandBuffer& buffer, std::optional<std::uint32_t> index);
    void next_command_buffer(const CommandBuffer& buffer);
} // namespace VSL_NAMESPACE::lifetime
