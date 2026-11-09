#pragma once
#include "../define.hpp"
#include "../vulkan/pv.hpp"
#include <memory>
#include <cstdint>

namespace VSL_NAMESPACE {
    // フェンスが再利用されても、個々の送信の完了状態を維持する。
    struct Completion {
        enum class Status {
            PENDING,
            SUBMITTED,
            COMPLETE,
            CANCELLED,
            FAILED
        };
        enum class Failure {
            NONE,
            SUBMIT_FAILED,
            DEVICE_LOST
        };
        std::shared_ptr<impl::SubmissionImpl> data_;
        Status status() const;
        Failure failure() const;
        std::int32_t error_code() const;
        bool ready() const;
        bool busy() const;
        bool native_uncertain() const;
        bool wait(std::uint64_t timeout_ns = UINT64_MAX) const;
        void retain(const std::shared_ptr<void>& resource) const;
        void retain(std::shared_ptr<void>&& resource) const;
        explicit operator bool() const { return bool(data_); }
        bool operator==(const Completion&) const = default;
    };

} // namespace VSL_NAMESPACE
