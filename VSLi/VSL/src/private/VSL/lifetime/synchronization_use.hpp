#pragma once
#include <VSL/vulkan/_pimpls.hpp>

namespace VSL_NAMESPACE::lifetime {
    void require_semaphore_owner(const std::shared_ptr<impl::SemaphoreImpl>& semaphore);
    std::shared_ptr<sync::FenceReservation> reserve_fence(std::shared_ptr<impl::FenceImpl> fence, std::uint32_t index,
                                                          const std::shared_ptr<impl::SubmissionImpl>& result);
    void require_fence_reservation(const impl::SubmissionWork& work, std::uint32_t index);
    bool wait_fences(std::shared_ptr<impl::FenceImpl> fence, std::optional<std::uint32_t> index, std::uint64_t timeout);
    void reset_fences(std::shared_ptr<impl::FenceImpl> fence, std::optional<std::uint32_t> index);
    void destroy_fences(std::shared_ptr<impl::FenceImpl> fence);
    void destroy_semaphores(std::shared_ptr<impl::SemaphoreImpl> semaphore);
} // namespace VSL_NAMESPACE::lifetime
