#pragma once
#include "../define.hpp"
#include "synchronize.hpp"
#include "device.hpp"
#include "swapchain.hpp"

namespace VSL_NAMESPACE {
    struct CommandManager;
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

        void next();

        void reset();

        void reset(std::uint32_t idx);

        std::shared_ptr<VSL_NAMESPACE::_impl::CommandBuffer_impl> _data;
    };

    struct PipelineAccessor;
    namespace command {
        struct __Command {
            virtual void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) = 0;
        };

        struct __DelegateCommand {
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
            virtual PipelineAccessor getPipeline() = 0;
        };

        struct __PipelineRequire {
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
            virtual size_t getVertexSize() = 0;
        };

        struct __VertexSizeRequire {
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
    }

    struct CommandManager {
        CommandManager(std::shared_ptr<VSL_NAMESPACE::_impl::CommandManager_impl> data);

        CommandManager(VSL_NAMESPACE::LogicalDeviceAccessor device);

        std::shared_ptr<VSL_NAMESPACE::_impl::CommandManager_impl> _data;

        template<class P = DefaultPhase, typename... Args>
        P startPhase(Args &&... args);

        CommandPool getPool();

        CommandBuffer getBuffer();

        std::uint32_t getCurrentBufferIdx();

        CommandBuffer makeExclusiveBuffer(size_t size = 1);
    };
}

// ============================================================================

template<class P, typename... Args>
P VSL_NAMESPACE::CommandManager::startPhase(Args&&... args)
{
    return P(*this, std::forward<Args>(args)...);
}