#pragma once
#include <functional>
#include <memory>
#include <utility>

namespace vsl_test {
    struct DeferredAction {
        std::shared_ptr<std::function<void()>> callback;
        explicit DeferredAction(std::function<void()> callback);
        std::shared_ptr<void> owner() const;
        ~DeferredAction() { *callback = {}; }
    };
    inline DeferredAction::DeferredAction(std::function<void()> callback)
        : callback(std::make_shared<std::function<void()>>(std::move(callback))) {}
    inline std::shared_ptr<void> DeferredAction::owner() const {
        return std::shared_ptr<void>(new int(0), [callback = callback](void* value) {
            delete static_cast<int*>(value);
            const auto invoke = *callback;
            if (invoke)
                invoke();
        });
    }
} // namespace vsl_test
