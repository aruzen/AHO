#pragma once
#include "../define.hpp"
#include "../sync/completion.hpp"
#include "synchronize.hpp"
#include "device.hpp"
#include "swapchain.hpp"
#include "../operation/command_create_info.hpp"
#include "../lifetime/owner_ref.hpp"

namespace VSL_NAMESPACE {
    namespace lifetime {
        struct CommandDomain;
        struct CommandAccess;
    } // namespace lifetime
    struct CommandManager;
    struct Completion;
    struct DefaultPhase;
    struct DefaultPhaseStream;

    inline std::uint32_t DEFAULT_BUFFER_SIZE = 3;

    struct CommandPool {
        CommandPool(VSL_NAMESPACE::LogicalDeviceAccessor device, CommandPoolCreateInfo info = {});

        CommandPool(std::shared_ptr<VSL_NAMESPACE::impl::CommandPoolImpl> data);

        std::shared_ptr<VSL_NAMESPACE::impl::CommandPoolImpl> data_;
        LogicalDeviceAccessor get_device() const;
        lifetime::OwnerRef owner_ref() const;
    };

    struct CommandBuffer {
        CommandBuffer(VSL_NAMESPACE::CommandPool pool, std::uint32_t size = DEFAULT_BUFFER_SIZE);

        CommandBuffer(std::shared_ptr<VSL_NAMESPACE::impl::CommandBufferImpl> data);
        CommandBuffer(const CommandBuffer&) = default;
        CommandBuffer(CommandBuffer&&) noexcept = default;
        CommandBuffer& operator=(CommandBuffer other) noexcept;
        void swap(CommandBuffer& other) noexcept;
        CommandPool get_pool() const;
        lifetime::OwnerRef owner_ref() const;

        std::uint32_t get_current_buffer_idx();

        size_t get_size();

        // Phaseの記録中だけ、その送信の完了状態を返す。
        Completion completion() const;

        void next();

        void reset();

        void reset(std::uint32_t idx);

        // Phaseの記録中は、送信完了まで使用する所有者を保持する。
        void retain(const std::shared_ptr<void>& resource) const;
        void retain(std::shared_ptr<void>&& resource) const;
        // 新しい保持契約。完了結果の観測とは別に、完了/取消時に解放する。
        void retain_until_complete(const std::shared_ptr<void>& resource) const;
        void retain_until_complete(std::shared_ptr<void>&& resource) const;

        std::shared_ptr<VSL_NAMESPACE::impl::CommandBufferImpl> data_;

    private:
        std::shared_ptr<lifetime::CommandDomain> _domain;
        friend struct lifetime::CommandAccess;
    };

    struct PipelineAccessor;
    namespace command {
        struct Command {
            virtual ~Command() = default;
            virtual void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) = 0;
        };

        struct DelegateCommand {
            virtual ~DelegateCommand() = default;
            virtual void invoke(DefaultPhaseStream& pst, CommandPool pool, CommandBuffer buffer, CommandManager manager) = 0;
        };

        template<typename T>
        concept is_command = requires(T t) {
            t.invoke(std::declval<CommandPool>(), std::declval<CommandBuffer>(), std::declval<CommandManager>());
        };

        template<typename T>
        concept is_delegate_command = requires(T t) {
            t.invoke(std::declval<DefaultPhaseStream&>(), std::declval<CommandPool>(), std::declval<CommandBuffer>(), std::declval<CommandManager>());
        };

        struct PipelineHolder {
            virtual ~PipelineHolder() = default;
            virtual PipelineAccessor get_pipeline() = 0;
        };

        struct PipelineRequire {
            virtual ~PipelineRequire() = default;
            virtual void set_pipeline(std::optional<PipelineAccessor> pipeline) = 0;
        };

        template<typename T>
        concept is_pipeline_holder = requires(T t) {
            { t.get_pipeline() } -> std::convertible_to<PipelineAccessor>;
        };

        template<typename T>
        concept is_pipeline_require = requires(T t) {
            { t.set_pipeline(std::declval<std::optional<PipelineAccessor>>()) };
        };

        struct VertexSizeHolder {
            virtual ~VertexSizeHolder() = default;
            virtual size_t get_vertex_size() = 0;
        };

        struct VertexSizeRequire {
            virtual ~VertexSizeRequire() = default;
            virtual void set_vertex_size(std::optional<size_t> vertex_size) = 0;
        };

        template<typename T>
        concept is_vertex_size_holder = requires(T t) {
            { t.get_vertex_size() } -> std::convertible_to<size_t>;
        };

        template<typename T>
        concept is_vert_size_require = requires(T t) {
            { t.set_vertex_size(std::declval<std::optional<size_t>>()) };
        };
    } // namespace command

    struct CommandManager {
        CommandManager(std::shared_ptr<VSL_NAMESPACE::impl::CommandManagerImpl> data);

        CommandManager(VSL_NAMESPACE::LogicalDeviceAccessor device);
        static CommandManager FromBuffer(CommandBuffer buffer);
        CommandManager(const CommandManager&) = default;
        CommandManager(CommandManager&&) noexcept = default;
        CommandManager& operator=(CommandManager other) noexcept;
        void swap(CommandManager& other) noexcept;

        std::shared_ptr<VSL_NAMESPACE::impl::CommandManagerImpl> data_;

        template<class P = DefaultPhase, typename... Args>
        P start_phase(Args&&... args);

        LogicalDeviceAccessor get_device() const;
        // 同じデバイスとグラフィックスキューで、プールとコマンドバッファを独立させる。
        CommandManager independent(std::uint32_t buffer_count = 1) const;
        void wait_idle();

        CommandPool get_pool();

        CommandBuffer get_buffer();

        std::uint32_t get_current_buffer_idx();

        CommandBuffer make_exclusive_buffer(size_t size = 1);

    private:
        std::shared_ptr<lifetime::CommandDomain> _domain;
        friend struct lifetime::CommandAccess;
    };

} // namespace VSL_NAMESPACE

template<class P, typename... Args>
P VSL_NAMESPACE::CommandManager::start_phase(Args&&... args) {
    return P(*this, std::forward<Args>(args)...);
}
