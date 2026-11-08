#include <VSL/lifetime/synchronization_use.hpp>
#include <VSL/lifetime/synchronization_storage.hpp>
#include <VSL/vulkan/synchronize.hpp>
#include <VSL/operation/synchronization_ops.hpp>
#include <VSL/sync/submission_completion.hpp>
#include <algorithm>
#include <stdexcept>

namespace VSL_NAMESPACE::lifetime {
    namespace {
        struct FenceAccess {
            std::shared_ptr<impl::FenceImpl> data;
            std::vector<vk::Fence> handles;
            std::vector<std::shared_ptr<impl::SubmissionImpl>> results;
            size_t first = 0, end = 0;
            bool active = false;
            FenceAccess(std::shared_ptr<impl::FenceImpl> data, std::optional<std::uint32_t> index);
            ~FenceAccess();
            void check() const;
            void require_idle(bool wait) const;
            void require_waitable() const;
        };
        struct SemaphoreAccess {
            sync::SemaphoreUseState& state;
            explicit SemaphoreAccess(sync::SemaphoreUseState& state);
            ~SemaphoreAccess() { state.accessing = false; }
        };
        template<class Map, class T>
        void erase_target(Map& map, const std::shared_ptr<T>& data);
        void append_result(std::vector<std::shared_ptr<impl::SubmissionImpl>>& results, std::shared_ptr<impl::SubmissionImpl> value);

        void append_result(std::vector<std::shared_ptr<impl::SubmissionImpl>>& results, std::shared_ptr<impl::SubmissionImpl> value) {
            if (value && std::ranges::find(results, value) == results.end())
                results.push_back(std::move(value));
        }
        template<class Map, class T>
        void erase_target(Map& map, const std::shared_ptr<T>& data) {
            std::erase_if(map, [&](const auto& item) { return item.second == data; });
        }
        FenceAccess::FenceAccess(std::shared_ptr<impl::FenceImpl> data, std::optional<std::uint32_t> index) : data(std::move(data)) {
            if (!this->data || this->data->fences.empty() || this->data->uses.size() != this->data->fences.size())
                throw std::logic_error("Fence has been destroyed or its state is invalid");
            (void)FenceHolder{this->data}.owner_ref();
            if (index && *index >= this->data->fences.size())
                throw std::out_of_range("Fence index");
            first = index.value_or(0);
            end = index ? size_t(*index) + 1 : this->data->fences.size();
            handles.assign(this->data->fences.begin() + first, this->data->fences.begin() + end);
            results.reserve(2 * handles.size());
            for (size_t i = first; i < end; ++i) {
                const auto& use = this->data->uses[i];
                if (use.accessing)
                    throw std::logic_error("Fence element is being accessed");
                if (const auto reservation = use.reservation.lock())
                    append_result(results, reservation->result.lock());
                append_result(results, use.submitted.lock());
            }
            for (size_t i = first; i < end; ++i)
                this->data->uses[i].accessing = true;
            active = true;
        }
        FenceAccess::~FenceAccess() {
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
        void FenceAccess::require_waitable() const {
            for (const auto& result : results)
                if (result->status == Completion::Status::PENDING)
                    throw std::logic_error("Fence is reserved by an unsubmitted phase");
        }
        void FenceAccess::require_idle(bool wait) const {
            require_waitable();
            for (const auto& result : results) {
                const auto status = Completion{result}.status();
                if (status == Completion::Status::SUBMITTED && (!wait || !Completion{result}.wait()))
                    throw std::logic_error("Fence is in GPU use");
            }
            check();
        }
        SemaphoreAccess::SemaphoreAccess(sync::SemaphoreUseState& state) : state(state) {
            if (state.accessing)
                throw std::logic_error("Semaphore is being accessed");
            state.accessing = true;
        }
    } // namespace
    void require_semaphore_owner(const std::shared_ptr<impl::SemaphoreImpl>& semaphore) {
        (void)SemaphoreHolder{semaphore}.owner_ref();
    }
    std::shared_ptr<sync::FenceReservation> reserve_fence(std::shared_ptr<impl::FenceImpl> fence, std::uint32_t index,
                                                          const std::shared_ptr<impl::SubmissionImpl>& result) {
        const FenceAccess access(std::move(fence), index);
        if (!result || result->status != Completion::Status::PENDING)
            throw std::logic_error("Fence reservation requires a pending result");
        auto reservation = std::make_shared<sync::FenceReservation>();
        reservation->result = result;
        access.require_idle(true);
        access.data->uses.at(index).reservation = reservation;
        return reservation;
    }
    void require_fence_reservation(const impl::SubmissionWork& work, std::uint32_t index) {
        if (!work.external_fence)
            return;
        const auto& fence = *work.external_fence;
        if (index >= fence.fences.size() || index >= fence.uses.size() || fence.uses[index].accessing ||
            !work.fence_reservation || fence.uses[index].reservation.lock() != work.fence_reservation ||
            work.fence_reservation->result.lock() != work.result || work.fence != fence.fences[index])
            throw std::logic_error("Phase no longer owns its fence reservation");
    }
    bool wait_fences(std::shared_ptr<impl::FenceImpl> fence, std::optional<std::uint32_t> index, std::uint64_t timeout) {
        const FenceAccess access(std::move(fence), index);
        access.require_waitable();
        const auto result = operation::wait_fences(access.data->native_device, access.handles, timeout);
        if (result == vk::Result::eTimeout)
            return false;
        if (result == vk::Result::eErrorDeviceLost)
            for (const auto& completion : access.results)
                if (const auto work = completion->work.lock())
                    sync::observe_submission_result(*work, static_cast<std::int32_t>(result));
        if (result != vk::Result::eSuccess)
            vk::detail::throwResultException(result, "Fence wait failed");
        for (const auto& completion : access.results)
            (void)Completion{completion}.status();
        access.check();
        return true;
    }
    void reset_fences(std::shared_ptr<impl::FenceImpl> fence, std::optional<std::uint32_t> index) {
        const FenceAccess access(std::move(fence), index);
        access.require_idle(false);
        operation::reset_fences(access.data->native_device, access.handles);
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
            use.reservation.reset();
            use.submitted.reset();
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
        if (registry)
            erase_target(registry->semaphores, semaphore);
    }
} // namespace VSL_NAMESPACE::lifetime
