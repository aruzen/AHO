//
// Created by morimoto_hibiki on 2025/07/30.
//

#ifndef AHO_ALL_PHASE_HPP
#define AHO_ALL_PHASE_HPP
#include "../define.h"
#include "pv.h"

#include "command.h"
#include "pipeline.h"

namespace VSL_NAMESPACE {
    struct DefaultPhase {
        DefaultPhase(CommandManager manager, /*SwapchainAccessor swapchain,*/
                     std::optional<SemaphoreHolder> nextImageAvailable = std::nullopt,
                     std::optional<SemaphoreHolder> calculationFinish = std::nullopt,
                     std::optional<FenceHolder> inFlightFence = std::nullopt);

        CommandManager manager;
        // SwapchainAccessor swapchain;
        std::optional<SemaphoreHolder> nextImageAvailable = std::nullopt, calculationFinish = std::nullopt;
        std::optional<FenceHolder> inFlightFence = std::nullopt;

        std::uint32_t imageIndex;

        DefaultPhaseStreamOperator operator<<(std::shared_ptr<command::__Command> cmd);
        DefaultPhaseStreamOperator operator<<(std::shared_ptr<command::__Manipulator> cmd);
        template <typename T>
        requires (command::is_command<T>) || (command::is_manipulator<T>)
        DefaultPhaseStreamOperator operator<<(T cmd);

        std::uint32_t getImageIndex();

        virtual ~DefaultPhase();
    };

    struct DefaultPhaseStreamOperator {
        DefaultPhase* parent;

        std::optional<size_t> vertexSize;
        std::optional<PipelineAccessor> pipeline;

        DefaultPhaseStreamOperator& operator<<(std::shared_ptr<command::__Command> cmd);
        template <command::is_command T>
        DefaultPhaseStreamOperator& operator<<(T cmd);
        DefaultPhaseStreamOperator& operator<<(std::shared_ptr<command::__Manipulator> manip);
        template <command::is_manipulator T>
        DefaultPhaseStreamOperator& operator<<(T manip);
    private:
        template<typename T>
        void resolveHolderRequire(T& t);
        template<typename T>
        void resolveHolderRequirePtr(std::shared_ptr<T> cmd);
    };

    /*
    struct ComputePhaseStreamOperator;
    struct ComputePhase {
        ComputePhase(CommandManager manager,
                     std::optional<SemaphoreHolder> nextImageAvailable = std::nullopt,
                     std::optional<SemaphoreHolder> calculationFinish = std::nullopt,
                     std::optional<FenceHolder> inFlightFence = std::nullopt);

        CommandManager manager;
        std::optional<SemaphoreHolder> nextImageAvailable = std::nullopt, calculationFinish = std::nullopt;
        std::optional<FenceHolder> inFlightFence = std::nullopt;

        std::uint32_t imageIndex;

        ComputePhaseStreamOperator operator<<(std::shared_ptr<command::__Command> cmd);
        ComputePhaseStreamOperator operator<<(std::shared_ptr<command::__Manipulator> cmd);
        template <typename T>
        requires (command::is_command<T>) || (command::is_manipulator<T>)
        ComputePhaseStreamOperator operator<<(T cmd);

        std::uint32_t getImageIndex();

        virtual ~ComputePhase();
    };

    struct ComputePhaseStreamOperator {
        DefaultPhase* parent;

        std::optional<size_t> vertexSize;
        std::optional<PipelineAccessor> pipeline;

        ComputePhaseStreamOperator& operator<<(std::shared_ptr<command::__Command> cmd);
        template <command::is_command T>
        ComputePhaseStreamOperator& operator<<(T cmd);
        ComputePhaseStreamOperator& operator<<(std::shared_ptr<command::__Manipulator> manip);
        template <command::is_manipulator T>
        ComputePhaseStreamOperator& operator<<(T manip);
    private:
        template<typename T>
        void resolveHolderRequire(T& t);
        template<typename T>
        void resolveHolderRequirePtr(std::shared_ptr<T> cmd);
    };
     */

    // ============================================================================

    template<class P, typename... Args>
    P VSL_NAMESPACE::CommandManager::startPhase(Args&&... args)
    {
        return P(*this, std::forward<Args>(args)...);
    }

    template <typename T>
    requires (command::is_command<T>) || (command::is_manipulator<T>)
    DefaultPhaseStreamOperator DefaultPhase::operator<<(T cmd)
    {
        DefaultPhaseStreamOperator op{ this };
        op << cmd;
        return op;
    }

    template<command::is_command T>
    DefaultPhaseStreamOperator& DefaultPhaseStreamOperator::operator<<(T cmd)
    {
        resolveHolderRequire(cmd);
        (&cmd)->invoke(parent->manager.getPool(), parent->manager.getBuffer(), parent->manager);
        return *this;
    }

    template<command::is_manipulator T>
    DefaultPhaseStreamOperator& DefaultPhaseStreamOperator::operator<<(T manip)
    {
        resolveHolderRequire(manip);
        (&manip)->manipulate(this, parent->manager.getPool(), parent->manager.getBuffer(), parent->manager);
        return *this;
    }
}

template<typename T>
void vsl::DefaultPhaseStreamOperator::resolveHolderRequire(T &t) {
    if constexpr (command::is_vertex_size_holder<T>)
        this->vertexSize = (&t)->getVertexSize();
    if constexpr (command::is_pipeline_holder<T>)
        this->pipeline = (&t)->getPipeline();
    if constexpr (std::convertible_to<T, PipelineAccessor>)
        this->pipeline = t;

    if constexpr (command::is_vertex_size_holder<T>)
        (&t)->setPipeline(this->vertexSize);
    if constexpr (command::is_pipeline_require<T>)
        (&t)->setPipeline(this->pipeline);
}


template<typename T>
void vsl::DefaultPhaseStreamOperator::resolveHolderRequirePtr(std::shared_ptr<T> cmd) {
    if (auto holder = std::dynamic_pointer_cast<command::__VertexSizeRequire>(cmd); holder)
        holder->setVertexSize(this->vertexSize);
    if (auto holder = std::dynamic_pointer_cast<command::__PipelineRequire>(cmd); holder)
        holder->setPipeline(this->pipeline);

    if (auto holder = std::dynamic_pointer_cast<command::__VertexSizeHolder>(cmd); holder)
        this->vertexSize = holder->getVertexSize();
    if (auto holder = std::dynamic_pointer_cast<command::__PipelineHolder>(cmd); holder)
        this->pipeline = holder->getPipeline();
}

#endif //AHO_ALL_PHASE_HPP
