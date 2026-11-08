#include <VSL/lifetime/logical_use_state.hpp>
#include <stdexcept>

namespace VSL_NAMESPACE::lifetime {
    LogicalUseState::LogicalUseState() : _state(std::make_shared<ResourceUseState>()) {}
    ResourceUse LogicalUseState::resource_use() const {
        if (!_state)
            throw std::logic_error("Logical use state is empty");
        return {_state, {}};
    }
    bool LogicalUseState::busy() const {
        return resource_use().state->busy();
    }
    bool LogicalUseState::wait(std::uint64_t timeout) const {
        return resource_use().state->wait(timeout);
    }
} // namespace VSL_NAMESPACE::lifetime
