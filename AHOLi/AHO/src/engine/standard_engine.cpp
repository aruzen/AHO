#include <AHO/engine/standard_engine.hpp>
#include "../window_impl.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {
    using AHO_NAMESPACE::engine::EngineConfig;
    using AHO_NAMESPACE::engine::EngineData;
    using AHO_NAMESPACE::engine::StandardEngineConfig;

    struct BootResources {
        std::shared_ptr<EngineData> engine;
        std::shared_ptr<VSL_NAMESPACE::Surface> surface;
    };

    void validate_config(const StandardEngineConfig& config);
    BootResources create_engine_resources(const EngineConfig& config, VSL_NAMESPACE::Window& window);

    void validate_config(const StandardEngineConfig& config) {
        if (config.engine.application_name.empty())
            throw std::invalid_argument("StandardEngine requires an application name");
        if (config.initial_window.width <= 0 || config.initial_window.height <= 0)
            throw std::invalid_argument("StandardEngine requires positive initial window dimensions");
        std::size_t remaining = std::numeric_limits<std::uint32_t>::max();
        for (const auto& [type, amount] : config.engine.descriptor_budget) {
            if (type > VSL_NAMESPACE::graphic_resource::Type::INPUT_ATTACHMENT || amount == 0 || amount > remaining)
                throw std::invalid_argument("StandardEngine requires valid, positive descriptor budgets");
            remaining -= amount;
        }
    }

    BootResources create_engine_resources(const EngineConfig& config, VSL_NAMESPACE::Window& window) {
        using namespace VSL_NAMESPACE;
        std::vector<const char*> extensions{"VK_KHR_get_physical_device_properties2"};
#ifdef __APPLE__
        extensions.push_back("VK_KHR_portability_enumeration");
#endif
        for (const auto& extension : config.instance_extensions) {
            if (std::ranges::find(extensions, extension) == extensions.end())
                extensions.push_back(extension.c_str());
        }
        Vulkan<> instance(config.application_name.c_str(), extensions);
        auto surface = window.add_plugin<Surface>(instance);
        PhysicalDevices devices(instance);
        auto physical = config.physical_device_index ? devices.get().at(*config.physical_device_index) : devices.search();
        LogicalDevice device(physical, surface);
        return {std::make_shared<EngineData>(EngineData{
                    .vulkan_instance = instance,
                    .physical_device = physical,
                    .logical_device = device,
                    .command_manager = CommandManager(device),
                    .graphic_resource_manager = std::make_shared<graphic_resource::OverAllocateManager>(
                        device, config.descriptor_budget, 0),
                    .synchro_manager = SynchroManager(device)}),
                std::move(surface)};
    }
} // namespace

namespace AHO_NAMESPACE::engine {
    StandardEngine::StandardEngine(std::string application_name, bool boot_on)
        : StandardEngine(StandardEngineConfig{.engine = {.application_name = std::move(application_name)}}, boot_on) {}

    StandardEngine::StandardEngine(StandardEngineConfig config, bool boot_on) : _config(std::move(config)) {
        validate_config(_config);
        if (boot_on)
            boot();
    }

    void StandardEngine::boot() {
        if (data())
            throw std::logic_error("StandardEngine is already booted");
        validate_config(_config);
        const auto& initial = _config.initial_window;
        const auto& title = initial.title.empty() ? _config.engine.application_name : initial.title;
        VSL_NAMESPACE::Window native(title, initial.width, initial.height);
        try {
            auto resources = create_engine_resources(_config.engine, native);
            EngineAccessor handle(resources.engine);
            auto drawing = window::detail::make_window_data(handle, std::move(resources.surface), title);
            Window window(handle, native.data_, std::move(drawing));
            std::optional<DefaultEngineRegistration> registration;
            if (_config.register_as_default)
                registration.emplace(handle);
            boot_window.emplace(std::move(window));
            reset_data(std::move(resources.engine));
            _default_registration = std::move(registration);
        } catch (...) {
            native.data_->plugins.clear();
            native.close();
            throw;
        }
    }

    void StandardEngine::boot(const std::string& application_name) {
        if (data())
            throw std::logic_error("StandardEngine is already booted");
        if (application_name.empty())
            throw std::invalid_argument("StandardEngine requires an application name");
        _config.engine.application_name = application_name;
        boot();
    }

    const StandardEngineConfig& StandardEngine::config() const {
        return _config;
    }

    Window StandardEngine::get_window() {
        return boot_window.value();
    }
} // namespace AHO_NAMESPACE::engine
