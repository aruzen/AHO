#pragma once
#include "../define.hpp"
#include "synchronize.hpp"
#include "device.hpp"
#include "swapchain.hpp"

namespace VSL_NAMESPACE {
    struct CommandManager;
    struct Completion;
    struct DefaultPhase;
    struct DefaultPhaseStream;

    static std::uint32_t DEFAULT_BUFFER_SIZE = 3;

    struct CommandPool {
        CommandPool(VSL_NAMESPACE::LogicalDeviceAccessor device);

        CommandPool(std::shared_ptr<VSL_NAMESPACE::_impl::CommandPool_impl> data);

        std::shared_ptr<VSL_NAMESPACE::_impl::CommandPool_impl> _data;
    };

    struct CommandBuffer {
        CommandBuffer(VSL_NAMESPACE::CommandPool pool, std::uint32_t size = DEFAULT_BUFFER_SIZE);

        CommandBuffer(std::shared_ptr<VSL_NAMESPACE::_impl::CommandBuffer_impl> data);

        std::uint32_t getCurrentBufferIdx();

        size_t getSize();

        // Phaseの記録中だけ、その送信の完了状態を返す。
        Completion completion() const;

        void next();

        void reset();

        void reset(std::uint32_t idx);

        // Phaseの記録中は、送信完了まで使用する所有者を保持する。
        void retain(std::shared_ptr<void> resource) const;

        std::shared_ptr<VSL_NAMESPACE::_impl::CommandBuffer_impl> _data;
    };

    // フェンスが再利用されても、個々の送信の完了状態を維持する。
    struct Completion {
        enum class Status {
            Pending,
            Submitted,
            Complete,
            Cancelled
        };
        std::shared_ptr<_impl::Submission_impl> _data;
        Status status() const;
        bool ready() const;
        bool wait(std::uint64_t timeout_ns = UINT64_MAX) const;
        void retain(std::shared_ptr<void> resource) const;
        explicit operator bool() const;
        bool operator==(const Completion&) const = default;
    };

    struct PipelineAccessor;
    namespace command {
        struct __Command {
            virtual ~__Command() = default;
            virtual void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) = 0;
        };

        struct __DelegateCommand {
            virtual ~__DelegateCommand() = default;
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

        struct __PipelineHolder {
            virtual ~__PipelineHolder() = default;
            virtual PipelineAccessor getPipeline() = 0;
        };

        struct __PipelineRequire {
            virtual ~__PipelineRequire() = default;
            virtual void setPipeline(std::optional<PipelineAccessor> pipeline) = 0;
        };

        template<typename T>
        concept is_pipeline_holder = requires(T t) {
            { t.getPipeline() } -> std::convertible_to<PipelineAccessor>;
        };

        template<typename T>
        concept is_pipeline_require = requires(T t) {
            { t.setPipeline(std::declval<std::optional<PipelineAccessor>>()) };
        };

        struct __VertexSizeHolder {
            virtual ~__VertexSizeHolder() = default;
            virtual size_t getVertexSize() = 0;
        };

        struct __VertexSizeRequire {
            virtual ~__VertexSizeRequire() = default;
            virtual void setVertexSize(std::optional<size_t> vertexSize) = 0;
        };

        template<typename T>
        concept is_vertex_size_holder = requires(T t) {
            { t.getVertexSize() } -> std::convertible_to<size_t>;
        };

        template<typename T>
        concept is_vert_size_require = requires(T t) {
            { t.setVertexSize(std::declval<std::optional<size_t>>()) };
        };
    } // namespace command

    struct CommandManager {
        CommandManager(std::shared_ptr<VSL_NAMESPACE::_impl::CommandManager_impl> data);

        CommandManager(VSL_NAMESPACE::LogicalDeviceAccessor device);

        std::shared_ptr<VSL_NAMESPACE::_impl::CommandManager_impl> _data;

        template<class P = DefaultPhase, typename... Args>
        P startPhase(Args&&... args);

        LogicalDeviceAccessor getDevice() const;
        // 同じデバイスとグラフィックスキューで、プールとコマンドバッファを独立させる。
        CommandManager independent(std::uint32_t bufferCount = 1) const;
        void waitIdle();

        CommandPool getPool();

        CommandBuffer getBuffer();

        std::uint32_t getCurrentBufferIdx();

        CommandBuffer makeExclusiveBuffer(size_t size = 1);
    };

    inline Completion::operator bool() const {
        return bool(_data);
    }
} // namespace VSL_NAMESPACE

template<class P, typename... Args>
P VSL_NAMESPACE::CommandManager::startPhase(Args&&... args) {
    return P(*this, std::forward<Args>(args)...);
}
