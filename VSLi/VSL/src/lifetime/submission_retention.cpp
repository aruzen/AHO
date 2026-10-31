#include <VSL/lifetime/submission_retention.hpp>
#include <VSL/vulkan/_pimpls.hpp>

namespace VSL_NAMESPACE::lifetime {
    void release_completed_resources(impl::SubmissionImpl& submission) noexcept {
        submission.release_resources = nullptr;
        submission.completion_resources.clear();
    }
} // namespace VSL_NAMESPACE::lifetime
