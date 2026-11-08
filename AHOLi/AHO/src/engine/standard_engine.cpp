#include <AHO/engine/standard_engine.hpp>
#include "../window_impl.hpp"
#include <VSL/lifetime/startup_arena.hpp>
#include <VSL/lifetime/glfw_lease.hpp>
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
        InstanceCreateInfo instance_info;
        instance_info.application_name = config.application_name;
        instance_info.validation = VALIDATION;
        instance_info.debug_printf = VALIDATION;
        instance_info.extensions = {"VK_KHR_get_physical_device_properties2"};
#ifdef __APPLE__
        instance_info.extensions.push_back("VK_KHR_portability_enumeration");
#endif
        for (const auto& extension : config.instance_extensions) {
            if (std::ranges::find(instance_info.extensions, extension) == instance_info.extensions.end())
                instance_info.extensions.push_back(extension);
        }
        const std::array instance_requests{storage::StartupRequest{{0}, storage::StartupKind::INSTANCE}};
        const storage::StartupPlan<1> instance_plan(instance_requests);
        const std::array<lifetime::StartupInit, 1> instance_inputs{lifetime::glfw_instance_info(std::move(instance_info))};
        lifetime::StartupArena instance_arena(instance_plan, instance_inputs);
        Vulkan<> instance(instance_arena.instance({0}));
        auto surface = AHO_NAMESPACE::window::detail::make_window_surface(window, instance);
        const std::array device_requests{storage::StartupRequest{{1}, storage::StartupKind::PHYSICAL_DEVICE},
                                         storage::StartupRequest{{2}, storage::StartupKind::DEVICE}};
        const storage::StartupPlan<2> device_plan(device_requests);
        const std::array<lifetime::StartupInit, 2> device_inputs{
            lifetime::PhysicalDeviceInit{instance, config.physical_device_index},
            lifetime::GraphicsDeviceInit{typed::ResourceId{1}, *surface, VALIDATION}};
        lifetime::StartupArena device_arena(device_plan, device_inputs);
        auto physical = device_arena.physical_device({1});
        auto device = device_arena.device({2});
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
        const auto native_data = native.data_;
        const auto os = native.window_owner();
        try {
            auto resources = create_engine_resources(_config.engine, native);
            if (native.data_ != native_data || !native_data->window_handle || native_data->window_handle != os.native_handle())
                throw std::runtime_error("Window closed during engine creation");
            EngineAccessor handle(resources.engine);
            auto drawing = window::detail::make_window_data(handle, std::move(resources.surface), title);
            if (native.data_ != native_data || !native_data->window_handle || native_data->window_handle != os.native_handle())
                throw std::runtime_error("Window closed during drawing resource creation");
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
