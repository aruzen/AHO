//
// Created by morimoto_hibiki on 2025/07/30.
//

#ifndef AHO_ALL_PHASE_HPP
#define AHO_ALL_PHASE_HPP

#include "../define.hpp"
#include "pv.hpp"

#include "command.hpp"
#include "pipeline.hpp"

namespace VSL_NAMESPACE {
    template<typename PhaseStream, typename T>
    concept is_phase_stream = requires(PhaseStream pst) {
        { pst << std::declval<std::shared_ptr<T>>() } -> std::convertible_to<PhaseStream&>;
        { pst << std::declval<T>() } -> std::convertible_to<PhaseStream&>;
    };

    struct DefaultPhaseStream;

    struct ComputePhase {
        ComputePhase(CommandManager manager,
                     std::optional<SemaphoreHolder> nextImageAvailable = std::nullopt,
                     std::optional<SemaphoreHolder> calculationFinish = std::nullopt,
                     std::optional<FenceHolder> inFlightFence = std::nullopt,
                     bool postponeSetup = false);

        CommandManager manager;
        std::optional<SemaphoreHolder> nextImageAvailable = std::nullopt, calculationFinish = std::nullopt;
        std::optional<FenceHolder> inFlightFence = std::nullopt;

        enum class State {
            Error,
            BeforeSetup,
            WaitSubmit,
            FinishSubmit
        } state = State::BeforeSetup;

        enum class SubmitResult {
            Success,
            Failed,
            WantRecreateSwapchain,
        };

        std::uint32_t imageIndex;

        DefaultPhaseStream operator<<(std::shared_ptr<command::__Command> cmd);

        DefaultPhaseStream operator<<(command::is_command auto cmd);

        DefaultPhaseStream operator<<(std::shared_ptr<command::__DelegateCommand> cmd);

        DefaultPhaseStream operator<<(command::is_delegate_command auto cmd);

        std::uint32_t getImageIndex();

        virtual void setup();

        virtual SubmitResult submit();

        // virtualはつけていないsubmitに委任
        ~ComputePhase();
    };

    struct DefaultPhase : public ComputePhase {
        DefaultPhase(CommandManager manager, SwapchainAccessor swapchain,
                     std::optional<SemaphoreHolder> nextImageAvailable = std::nullopt,
                     std::optional<SemaphoreHolder> calculationFinish = std::nullopt,
                     std::optional<FenceHolder> inFlightFence = std::nullopt,
                     bool postponeSetup = false);

        SwapchainAccessor swapchain;

        void setup() override;

        SubmitResult submit() override;

        ~DefaultPhase();
    };

    struct DefaultPhaseStream {
        CommandManager manager;

        std::optional<size_t> vertexSize;
        std::optional<PipelineAccessor> pipeline;

        DefaultPhaseStream &operator<<(std::shared_ptr<command::__Command> cmd);

        DefaultPhaseStream &operator<<(command::is_command auto cmd);

        DefaultPhaseStream &operator<<(std::shared_ptr<command::__DelegateCommand> cmd);

        DefaultPhaseStream &operator<<(command::is_delegate_command auto cmd);
    private:
        template<typename T>
        void resolveHolderRequire(T &t);

        template<typename T>
        void resolveHolderRequirePtr(std::shared_ptr<T> cmd);
    };

    // ============================================================================

    DefaultPhaseStream ComputePhase::operator<<(command::is_command auto cmd) {
        DefaultPhaseStream op{this->manager};
        op << cmd;
        return op;
    }

    DefaultPhaseStream ComputePhase::operator<<(command::is_delegate_command auto cmd) {
        DefaultPhaseStream op{this->manager};
        op << cmd;
        return op;
    }

    DefaultPhaseStream &DefaultPhaseStream::operator<<(command::is_command auto cmd) {
        resolveHolderRequire(cmd);
        (&cmd)->invoke(manager.getPool(), manager.getBuffer(), manager);
        return *this;
    }

    DefaultPhaseStream &DefaultPhaseStream::operator<<(command::is_delegate_command auto cmd) {
        resolveHolderRequire(cmd);
        (&cmd)->invoke(*this, manager.getPool(), manager.getBuffer(), manager);
        return *this;
    }
}

template<typename T>
void vsl::DefaultPhaseStream::resolveHolderRequire(T &t) {
    if constexpr (command::is_vertex_size_holder<T>)
        this->vertexSize = (&t)->getVertexSize();
    if constexpr (command::is_pipeline_holder<T>)
        this->pipeline = (&t)->getPipeline();
    if constexpr (std::convertible_to<T, PipelineAccessor>)
        this->pipeline = t;

    if constexpr (command::is_vertex_size_holder<T>)
        (&t)->setVertexSize(this->vertexSize);
    if constexpr (command::is_pipeline_require<T>)
        (&t)->setPipeline(this->pipeline);
}


template<typename T>
void vsl::DefaultPhaseStream::resolveHolderRequirePtr(std::shared_ptr<T> cmd) {
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
