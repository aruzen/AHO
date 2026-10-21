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
                     std::optional<SemaphoreHolder> next_image_available = std::nullopt,
                     std::optional<SemaphoreHolder> calculation_finish = std::nullopt,
                     std::optional<FenceHolder> in_flight_fence = std::nullopt,
                     bool postpone_setup = false,
                     PipelineStage wait_stage = PipelineStage::ALL_COMMANDS);

        CommandManager manager;
        std::optional<SemaphoreHolder> next_image_available = std::nullopt, calculation_finish = std::nullopt;
        std::optional<FenceHolder> in_flight_fence = std::nullopt;

        enum class State {
            ERROR_,
            BEFORE_SETUP,
            WAIT_SUBMIT,
            FINISH_SUBMIT
        } state = State::BEFORE_SETUP;

        enum class SubmitResult {
            SUCCESS,
            FAILED,
            WANT_RECREATE_SWAPCHAIN,
        };

        std::uint32_t image_index = UINT32_MAX;
        std::uint32_t frame_slot = 0;
        PipelineStage wait_stage;
        Completion completion() const;
        virtual void cancel();
        ComputePhase(const ComputePhase&) = delete;
        ComputePhase& operator=(const ComputePhase&) = delete;
        ComputePhase(ComputePhase&&) = delete;
        ComputePhase& operator=(ComputePhase&&) = delete;

        DefaultPhaseStream operator<<(std::shared_ptr<command::Command> cmd);

        DefaultPhaseStream operator<<(command::is_command auto cmd);

        DefaultPhaseStream operator<<(std::shared_ptr<command::DelegateCommand> cmd);

        DefaultPhaseStream operator<<(command::is_delegate_command auto cmd);

        std::uint32_t get_image_index();

        virtual void setup();

        virtual SubmitResult submit();

        virtual ~ComputePhase();

    protected:
        Completion completion_;
        std::uint32_t signal_index = 0;
        bool recreate = false;
        int initial_exceptions = 0;
        void prepare();
        void begin();
        void finish_noexcept() noexcept;
    };

    struct DefaultPhase : public ComputePhase {
        DefaultPhase(CommandManager manager, SwapchainAccessor swapchain,
                     std::optional<SemaphoreHolder> next_image_available = std::nullopt,
                     std::optional<SemaphoreHolder> calculation_finish = std::nullopt,
                     std::optional<FenceHolder> in_flight_fence = std::nullopt,
                     bool postpone_setup = false);

        SwapchainAccessor swapchain;

        void setup() override;

        std::uint32_t get_current_index();

        SubmitResult submit() override;
        // 読み戻しも記録してから送信する。画面表示は続けてpresent()で行う。
        virtual SubmitResult submit_commands();
        SubmitResult present();
        void cancel() override;

    private:
        bool _presented = false;
        struct Presentation;
        std::shared_ptr<Presentation> _presentation;

    public:
        ~DefaultPhase();
    };

    struct DefaultPhaseStream {
        CommandManager manager;

        std::optional<size_t> vertex_size;
        std::optional<PipelineAccessor> pipeline;

        DefaultPhaseStream& operator<<(std::shared_ptr<command::Command> cmd);

        DefaultPhaseStream& operator<<(command::is_command auto cmd);

        DefaultPhaseStream& operator<<(std::shared_ptr<command::DelegateCommand> cmd);

        DefaultPhaseStream& operator<<(command::is_delegate_command auto cmd);

    private:
        template<typename T>
        void resolve_holder_require(T& t);

        template<typename T>
        void resolve_holder_require_ptr(std::shared_ptr<T> cmd);
    };

    inline Completion ComputePhase::completion() const {
        return completion_;
    }

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

    DefaultPhaseStream& DefaultPhaseStream::operator<<(command::is_command auto cmd) {
        resolve_holder_require(cmd);
        (&cmd)->invoke(manager.get_pool(), manager.get_buffer(), manager);
        return *this;
    }

    DefaultPhaseStream& DefaultPhaseStream::operator<<(command::is_delegate_command auto cmd) {
        resolve_holder_require(cmd);
        (&cmd)->invoke(*this, manager.get_pool(), manager.get_buffer(), manager);
        return *this;
    }
} // namespace VSL_NAMESPACE

template<typename T>
void VSL_NAMESPACE::DefaultPhaseStream::resolve_holder_require(T& t) {
    if constexpr (command::is_vert_size_require<T>)
        (&t)->set_vertex_size(this->vertex_size);
    if constexpr (command::is_pipeline_require<T>)
        (&t)->set_pipeline(this->pipeline);

    if constexpr (command::is_vertex_size_holder<T>)
        this->vertex_size = (&t)->get_vertex_size();
    if constexpr (command::is_pipeline_holder<T>)
        this->pipeline = (&t)->get_pipeline();
    if constexpr (std::convertible_to<T, PipelineAccessor>)
        this->pipeline = t;
}

template<typename T>
void VSL_NAMESPACE::DefaultPhaseStream::resolve_holder_require_ptr(std::shared_ptr<T> cmd) {
    if (auto holder = std::dynamic_pointer_cast<command::VertexSizeRequire>(cmd); holder)
        holder->set_vertex_size(this->vertex_size);
    if (auto holder = std::dynamic_pointer_cast<command::PipelineRequire>(cmd); holder)
        holder->set_pipeline(this->pipeline);

    if (auto holder = std::dynamic_pointer_cast<command::VertexSizeHolder>(cmd); holder)
        this->vertex_size = holder->get_vertex_size();
    if (auto holder = std::dynamic_pointer_cast<command::PipelineHolder>(cmd); holder)
        this->pipeline = holder->get_pipeline();
}

#endif
