#include <VSL/lifetime/synchronization_use.hpp>
#include <VSL/lifetime/synchronization_storage.hpp>
#include <VSL/lifetime/submission_retention.hpp>
#include <VSL/lifetime/synchronization_access.hpp>
#include <VSL/operation/synchronization_ops.hpp>
#include <VSL/sync/submission_completion.hpp>
#include <VSL/sync/presentation_state.hpp>
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace VSL_NAMESPACE::lifetime {
    namespace {
        using FenceObservation = std::variant<std::monostate, std::shared_ptr<impl::SubmissionImpl>,
                                              std::shared_ptr<sync::FenceProof>, std::shared_ptr<sync::PresentState>>;
        struct FenceAccess {
            std::shared_ptr<impl::FenceImpl> data;
            std::vector<vk::Fence> handles;
            std::vector<FenceObservation> observations;
            size_t first = 0, end = 0;
            bool active = false, observing = false;
            FenceAccess(std::shared_ptr<impl::FenceImpl> data, std::optional<std::uint32_t> index);
            ~FenceAccess();
            void check() const;
            void require_idle(bool wait, bool poll_graphics = true) const;
            void require_waitable(bool poll) const;
            std::vector<std::shared_ptr<impl::SubmissionWork>> submitted_work() const;
            void save_observations(vk::Result result) const noexcept;
            void begin_observation() noexcept;
            void finish_observation() noexcept;
        };
        struct SemaphoreAccess {
            sync::SemaphoreUseState& state;
            explicit SemaphoreAccess(sync::SemaphoreUseState& state);
            ~SemaphoreAccess() { state.accessing = false; }
        };
        template<class Map, class T>
        void erase_target(Map& map, const std::shared_ptr<T>& data);
        FenceObservation snapshot_use(const sync::FenceUseState& use);
        void reset_reserved_fence(const FenceAccess& access, std::uint32_t index, sync::FenceUseState::Use use);
        bool idle_acquisition(sync::FenceProof& proof, bool wait) noexcept;
        bool idle_presentation(sync::PresentState& state, bool wait) noexcept;
        bool idle_acquisition(sync::FenceProof& proof, bool wait) noexcept {
            if (proof.observing)
                return false;
            if (proof.status == sync::FenceProofStatus::UNARMED)
                return false;
            if (!sync::fence_proof_terminal(proof))
                sync::poll_fence_proof(proof);
            if (!sync::fence_proof_terminal(proof) && wait)
                sync::wait_fence_proof(proof);
            return sync::fence_proof_terminal(proof);
        }
        bool idle_presentation(sync::PresentState& state, bool wait) noexcept {
            if (state.observing)
                return false;
            if (state.status == sync::PresentStatus::PENDING)
                return false;
            sync::poll_present(state);
            if (!sync::present_terminal(state) && wait && state.status == sync::PresentStatus::SUBMITTED)
                sync::wait_present(state);
            return sync::present_terminal(state);
        }
        FenceObservation snapshot_use(const sync::FenceUseState& use) {
            return std::visit([](const auto& source) -> FenceObservation {
                using Source = std::decay_t<decltype(source)>;
                if constexpr (std::is_same_v<Source, std::monostate>)
                    return {};
                else if constexpr (std::is_same_v<Source, std::weak_ptr<sync::FenceReservation>>) {
                    const auto reservation = source.lock();
                    return reservation ? FenceObservation(reservation->result.lock()) : FenceObservation{};
                } else
                    return source.lock();
            },
                              use.use);
        }
        void reset_reserved_fence(const FenceAccess& access, std::uint32_t index, sync::FenceUseState::Use use) {
            auto& target = access.data->uses.at(index).use;
            auto previous = std::exchange(target, std::move(use));
            try {
                operation::reset_fences(access.data->native_device, access.handles);
            } catch (...) {
                target = std::move(previous);
                throw;
            }
        }
        template<class Map, class T>
        void erase_target(Map& map, const std::shared_ptr<T>& data) {
            std::erase_if(map, [&](const auto& item) { return item.second == data; });
        }
        FenceAccess::FenceAccess(std::shared_ptr<impl::FenceImpl> data, std::optional<std::uint32_t> index) : data(std::move(data)) {
            if (!this->data || this->data->fences.empty() || this->data->uses.size() != this->data->fences.size())
                throw std::logic_error("Fence has been destroyed or its state is invalid");
            detail::SynchronizationAccess::RequireCanonical(this->data);
            if (index && *index >= this->data->fences.size())
                throw std::out_of_range("Fence index");
            first = index.value_or(0);
            end = index ? size_t(*index) + 1 : this->data->fences.size();
            handles.assign(this->data->fences.begin() + first, this->data->fences.begin() + end);
            observations.reserve(handles.size());
            for (size_t i = first; i < end; ++i) {
                const auto& use = this->data->uses[i];
                if (use.accessing)
                    throw std::logic_error("Fence element is being accessed");
                observations.push_back(snapshot_use(use));
            }
            for (size_t i = first; i < end; ++i)
                this->data->uses[i].accessing = true;
            active = true;
        }
        FenceAccess::~FenceAccess() {
            finish_observation();
            if (active)
                for (size_t i = first; i < end && i < data->uses.size(); ++i)
                    data->uses[i].accessing = false;
        }
        void FenceAccess::check() const {
            if (end > data->fences.size() || end > data->uses.size())
                throw std::logic_error("Fence storage changed while observing completion");
            for (size_t i = first; i < end; ++i)
                if (!data->uses[i].accessing || data->fences[i] != handles[i - first])
                    throw std::logic_error("Fence element changed while observing completion");
        }
        void FenceAccess::require_waitable(bool poll) const {
            for (const auto& observation : observations)
                std::visit([poll](const auto& value) {
                    using Value = std::decay_t<decltype(value)>;
                    if constexpr (!std::is_same_v<Value, std::monostate>) {
                        if (!value)
                            return;
                        if constexpr (std::is_same_v<Value, std::shared_ptr<impl::SubmissionImpl>>) {
                            if (value->native.active || value->native.observing || value->status == Completion::Status::PENDING)
                                throw std::logic_error("Fence is reserved by an unsubmitted phase");
                            if (!poll && (value->native.enqueue == sync::SubmissionEnqueue::UNKNOWN ||
                                          value->native.enqueue == sync::SubmissionEnqueue::NOT_ENQUEUED ||
                                          value->status == Completion::Status::CANCELLED))
                                throw std::logic_error("Fence enqueue has not been established; only polling is allowed");
                        } else if constexpr (std::is_same_v<Value, std::shared_ptr<sync::FenceProof>>) {
                            if (value->observing)
                                throw std::logic_error("Acquisition fence is being observed");
                            if (value->status == sync::FenceProofStatus::UNARMED || value->status == sync::FenceProofStatus::NOT_ACQUIRED)
                                throw std::logic_error("Fence has no submitted acquisition");
                            if (!poll && value->status == sync::FenceProofStatus::UNCERTAIN)
                                throw std::logic_error("Acquisition fence enqueue is uncertain; only polling is allowed");
                        } else if (value->observing || value->status == sync::PresentStatus::PENDING || (value->status == sync::PresentStatus::UNCERTAIN && !poll))
                            throw std::logic_error("Present fence enqueue has not been established");
                    }
                },
                           observation);
        }
        void FenceAccess::require_idle(bool wait, bool poll_graphics) const {
            for (const auto& observation : observations)
                std::visit([&](const auto& value) {
                    using Value = std::decay_t<decltype(value)>;
                    if constexpr (!std::is_same_v<Value, std::monostate>) {
                        if (!value)
                            return;
                        if constexpr (std::is_same_v<Value, std::shared_ptr<impl::SubmissionImpl>>) {
                            const Completion completion{value};
                            if (poll_graphics)
                                (void)completion.status();
                            if (sync::pending_use(*value) && wait && poll_graphics && !value->native.active &&
                                value->status == Completion::Status::SUBMITTED && value->native.enqueue == sync::SubmissionEnqueue::ENQUEUED)
                                (void)completion.wait();
                            if (sync::pending_use(*value))
                                throw std::logic_error("Fence is in graphics use");
                        } else if constexpr (std::is_same_v<Value, std::shared_ptr<sync::FenceProof>>) {
                            if (!idle_acquisition(*value, wait))
                                throw std::logic_error("Fence is in acquisition use");
                        } else {
                            if (!idle_presentation(*value, wait))
                                throw std::logic_error("Fence is in presentation use");
                        }
                    }
                },
                           observation);
            check();
        }
        std::vector<std::shared_ptr<impl::SubmissionWork>> FenceAccess::submitted_work() const {
            std::vector<std::shared_ptr<impl::SubmissionWork>> result;
            for (const auto& observation : observations)
                if (const auto* value = std::get_if<std::shared_ptr<impl::SubmissionImpl>>(&observation); value && *value)
                    if (auto work = (*value)->work.lock(); work && std::ranges::find(result, work) == result.end())
                        result.push_back(std::move(work));
            return result;
        }
        void FenceAccess::save_observations(vk::Result result) const noexcept {
            for (const auto& observation : observations)
                std::visit([&](const auto& value) {
                    using Value = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same_v<Value, std::shared_ptr<sync::FenceProof>>) {
                        if (value)
                            sync::save_fence_observation(*value, result);
                    } else if constexpr (std::is_same_v<Value, std::shared_ptr<sync::PresentState>>)
                        if (value)
                            sync::save_present_observation(*value, result);
                },
                           observation);
        }
        void FenceAccess::begin_observation() noexcept {
            observing = true;
            for (const auto& observation : observations)
                std::visit([](const auto& value) {
                    using Value = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same_v<Value, std::shared_ptr<sync::FenceProof>> || std::is_same_v<Value, std::shared_ptr<sync::PresentState>>)
                        if (value)
                            value->observing = true;
                },
                           observation);
        }
        void FenceAccess::finish_observation() noexcept {
            if (!observing)
                return;
            for (const auto& observation : observations)
                std::visit([](const auto& value) {
                    using Value = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same_v<Value, std::shared_ptr<sync::FenceProof>> || std::is_same_v<Value, std::shared_ptr<sync::PresentState>>)
                        if (value)
                            value->observing = false;
                },
                           observation);
            observing = false;
        }
        SemaphoreAccess::SemaphoreAccess(sync::SemaphoreUseState& state) : state(state) {
            if (state.accessing)
                throw std::logic_error("Semaphore is being accessed");
            state.accessing = true;
        }
    } // namespace
    void require_semaphore_owner(const std::shared_ptr<impl::SemaphoreImpl>& semaphore) {
        detail::SynchronizationAccess::RequireCanonical(semaphore);
    }
    std::shared_ptr<sync::FenceReservation> reserve_fence(std::shared_ptr<impl::FenceImpl> fence, std::uint32_t index,
                                                          const std::shared_ptr<impl::SubmissionImpl>& result) {
        const FenceAccess access(std::move(fence), index);
        if (!result || result->status != Completion::Status::PENDING)
            throw std::logic_error("Fence reservation requires a pending result");
        auto reservation = std::make_shared<sync::FenceReservation>();
        reservation->result = result;
        access.require_idle(true);
        access.data->uses.at(index).use = std::weak_ptr<sync::FenceReservation>(reservation);
        return reservation;
    }
    bool reserve_acquire_fence(std::shared_ptr<impl::FenceImpl> fence, std::uint32_t index, const std::shared_ptr<sync::FenceProof>& proof,
                               AcquireFenceGate gate) {
        const auto pending = proof;
        const FenceAccess access(std::move(fence), index);
        if (!pending || pending->status != sync::FenceProofStatus::UNARMED ||
            (pending->device && pending->device != access.data->native_device) || (pending->fence && pending->fence != access.handles[0]))
            throw std::invalid_argument("Acquisition requires a fresh proof for its fence element");
        access.require_idle(true);
        if (gate.ready && !gate.ready(gate.context))
            return false;
        pending->device = access.data->native_device;
        pending->fence = access.handles[0];
        try {
            reset_reserved_fence(access, index, std::weak_ptr<sync::FenceProof>(pending));
        } catch (...) {
            pending->status = sync::FenceProofStatus::NOT_ACQUIRED;
            throw;
        }
        return true;
    }
    void release_acquire_fence(impl::FenceImpl& fence, std::uint32_t index, const std::shared_ptr<sync::FenceProof>& proof) noexcept {
        if (!proof || proof->observing || !sync::fence_proof_terminal(*proof) || index >= fence.uses.size() || fence.uses[index].accessing)
            return;
        auto& use = fence.uses[index].use;
        const auto* source = std::get_if<std::weak_ptr<sync::FenceProof>>(&use);
        const auto current = source ? source->lock() : nullptr;
        if (current && current == proof && !current.owner_before(proof) && !proof.owner_before(current))
            use = std::monostate{};
    }
    void reserve_present_fence(std::shared_ptr<impl::FenceImpl> fence, std::uint32_t index, const std::shared_ptr<sync::PresentState>& state) {
        const FenceAccess access(std::move(fence), index);
        if (!state || !state->strict || state->status != sync::PresentStatus::PENDING || state->device != access.data->native_device ||
            (state->proof && state->proof != access.handles[0]))
            throw std::invalid_argument("Presentation requires a pending state for its fence element");
        // 世代poolの借出しで異種graphicsのcallbackを発火させない。
        access.require_idle(true, false);
        state->proof = access.handles[0];
        reset_reserved_fence(access, index, std::weak_ptr<sync::PresentState>(state));
    }
    void require_fence_reservation(const impl::SubmissionWork& work, std::uint32_t index) {
        if (!work.external_fence)
            return;
        const auto& fence = *work.external_fence;
        if (index >= fence.fences.size() || index >= fence.uses.size() || fence.uses[index].accessing ||
            !work.fence_reservation || sync::current_fence_reservation(fence.uses[index]) != work.fence_reservation ||
            work.fence_reservation->result.lock() != work.result || work.fence != fence.fences[index])
            throw std::logic_error("Phase no longer owns its fence reservation");
    }
    bool wait_fences(std::shared_ptr<impl::FenceImpl> fence, std::optional<std::uint32_t> index, std::uint64_t timeout) {
        FenceAccess access(std::move(fence), index);
        access.require_waitable(timeout == 0);
        const auto submitted = access.submitted_work();
        std::vector<SubmissionObservation> observations;
        observations.reserve(submitted.size());
        for (const auto& work : submitted)
            observations.emplace_back(*work);
        access.begin_observation();
        auto result = access.data->device->lost ? vk::Result::eErrorDeviceLost : operation::wait_fences(access.data->native_device, access.handles, timeout);
        if (access.data->device->lost)
            result = vk::Result::eErrorDeviceLost;
        if (result == vk::Result::eTimeout)
            return false;
        if (result == vk::Result::eSuccess || result == vk::Result::eErrorDeviceLost) {
            // 全観測値を保存してからcallbackを通知し、peerを再pollさせない。
            if (result == vk::Result::eErrorDeviceLost)
                access.data->device->lost = true;
            access.save_observations(result);
            for (const auto& work : submitted)
                sync::save_native_idle(*work, static_cast<std::int32_t>(result));
            for (auto& observation : observations)
                observation.finish();
            access.finish_observation();
            for (const auto& work : submitted)
                sync::finish_submission(*work);
        }
        if (result != vk::Result::eSuccess)
            vk::detail::throwResultException(result, "Fence wait failed");
        access.check();
        return true;
    }
    void reset_fences(std::shared_ptr<impl::FenceImpl> fence, std::optional<std::uint32_t> index) {
        const FenceAccess access(std::move(fence), index);
        access.require_idle(false);
        operation::reset_fences(access.data->native_device, access.handles);
        for (size_t i = access.first; i < access.end; ++i)
            access.data->uses[i].use = std::monostate{};
    }
    void destroy_fences(std::shared_ptr<impl::FenceImpl> fence) {
        if (!fence || fence->fences.empty())
            return;
        const auto registry = fence->owner_context ? fence->owner_context->registry.lock() : nullptr;
        const FenceAccess access(fence, std::nullopt);
        access.require_idle(false);
        operation::destroy_fences(fence->native_device, access.handles);
        fence->fences.clear();
        for (auto& use : fence->uses) {
            use.use = std::monostate{};
        }
        if (registry)
            erase_target(registry->fences, fence);
    }
    void destroy_semaphores(std::shared_ptr<impl::SemaphoreImpl> semaphore) {
        if (!semaphore || semaphore->semaphores.empty())
            return;
        require_semaphore_owner(semaphore);
        const auto registry = semaphore->owner_context ? semaphore->owner_context->registry.lock() : nullptr;
        const auto handles = semaphore->semaphores;
        const SemaphoreAccess access(semaphore->use);
        sync::require_semaphore_idle(semaphore->use);
        if (semaphore->semaphores != handles)
            throw std::logic_error("Semaphore storage changed while observing completion");
        operation::destroy_semaphores(semaphore->native_device, handles);
        semaphore->semaphores.clear();
        semaphore->use.graphics.clear();
        semaphore->use.wsi.clear();
        semaphore->use.poisoned.clear();
        if (registry)
            erase_target(registry->semaphores, semaphore);
    }
} // namespace VSL_NAMESPACE::lifetime
