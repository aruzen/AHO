#pragma once
#include <VSL/vulkan/_pimpls.hpp>

namespace VSL_NAMESPACE::lifetime {
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
    };
    struct CommandDomain {
        std::shared_ptr<impl::CommandBufferImpl> buffer;
        std::vector<std::shared_ptr<impl::SubmissionWork>> submissions;
        std::shared_ptr<impl::SubmissionWork> recording_submission;
        // graphics完了と区別し、Device idleまで保持する。厳密なWSI完了は後続段階。
        std::vector<std::shared_ptr<void>> presentation_resources;
        CommandRecordingContext context;

        explicit CommandDomain(std::shared_ptr<impl::CommandBufferImpl> buffer);
        CommandDomain(const CommandDomain&) = delete;
        CommandDomain& operator=(const CommandDomain&) = delete;
        ~CommandDomain();
        void wait_idle();
    };
    struct CommandAccess {
        static const std::shared_ptr<CommandDomain>& Owner(const CommandBuffer& buffer);
        static std::shared_ptr<CommandDomain> Resolve(const std::shared_ptr<impl::CommandBufferImpl>& buffer);
        static CommandBuffer Buffer(const std::shared_ptr<CommandDomain>& domain);
        static CommandManager Manager(std::shared_ptr<impl::CommandManagerImpl> data, std::shared_ptr<CommandDomain> domain);
    };
} // namespace VSL_NAMESPACE::lifetime
