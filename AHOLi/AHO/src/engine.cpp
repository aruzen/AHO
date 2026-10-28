#include <AHO/engine.hpp>
#include <algorithm>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace AHO_NAMESPACE::engine {
    struct DefaultEngineRegistration::Registration {
        EngineAccessor engine;
    };

    struct DefaultEngineRegistration::Registry {
        std::mutex mutex;
        std::vector<std::weak_ptr<Registration>> registrations;
    };

    EngineAccessor::EngineAccessor(std::shared_ptr<EngineData> data) : _data(std::move(data)) {}

    const std::shared_ptr<EngineData>& EngineAccessor::data() const {
        return _data;
    }

    void EngineAccessor::reset_data(std::shared_ptr<EngineData> data) {
        _data = std::move(data);
    }

    DefaultEngineRegistration::Registry& DefaultEngineRegistration::State() {
        static Registry REGISTRY;
        return REGISTRY;
    }

    DefaultEngineRegistration::DefaultEngineRegistration(EngineAccessor engine) {
        if (!engine.data())
            throw std::invalid_argument("DefaultEngineRegistration requires engine data");
        auto registration = std::make_shared<Registration>(std::move(engine));
        auto& state = State();
        std::lock_guard lock(state.mutex);
        std::erase_if(state.registrations, [](const auto& entry) { return entry.expired(); });
        state.registrations.push_back(registration);
        _registration = std::move(registration);
    }

    EngineAccessor DefaultEngineRegistration::Current() {
        auto& state = State();
        std::lock_guard lock(state.mutex);
        std::erase_if(state.registrations, [](const auto& entry) { return entry.expired(); });
        for (auto entry = state.registrations.rbegin(); entry != state.registrations.rend(); ++entry) {
            if (const auto registration = entry->lock())
                return registration->engine;
        }
        return {};
    }
} // namespace AHO_NAMESPACE::engine
