#pragma once
#include <VSL/vulkan/pv.hpp>

namespace VSL_NAMESPACE::lifetime {
    void release_completed_resources(impl::SubmissionImpl& submission) noexcept;
} // namespace VSL_NAMESPACE::lifetime
