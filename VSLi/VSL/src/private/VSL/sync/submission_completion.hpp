#pragma once
#include <VSL/vulkan/pv.hpp>

namespace VSL_NAMESPACE::sync {
    // 最終所有者から呼ばれ、送信の完了と観測結果を確定する。資源保持は解放しない。
    void finish_submission(impl::SubmissionImpl& submission) noexcept;
} // namespace VSL_NAMESPACE::sync
