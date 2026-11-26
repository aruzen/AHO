#include <AHO/engine/standard_engine.hpp>
#include "../window_impl.hpp"
#include <VSL/lifetime/startup_arena.hpp>
#include <VSL/lifetime/glfw_lease.hpp>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace {
    using AHO_NAMESPACE::engine::EngineConfig;
    using AHO_NAMESPACE::engine::EngineAccessor;
    using AHO_NAMESPACE::engine::StandardEngineConfig;

    struct BootResources {
        EngineAccessor engine;
        std::shared_ptr<VSL_NAMESPACE::Surface> surface;
    };

    void validate_config(const StandardEngineConfig& config);
    BootResources create_engine_resources(const EngineConfig& config, VSL_NAMESPACE::Window& window);

    void validate_config(const StandardEngineConfig& config) {
        if (config.engine.application_name.empty())
            throw std::invalid_argument("StandardEngine requires an application name");
        if (config.initial_window.width <= 0 || config.initial_window.height <= 0)
            throw std::invalid_argument("StandardEngine requires positive initial window dimensions");
        if (config.engine.present_fences != VSL_NAMESPACE::PresentFencePolicy::LEGACY &&
            config.engine.present_fences != VSL_NAMESPACE::PresentFencePolicy::REQUIRE_FENCE)
            throw std::invalid_argument("Unknown presentation fence policy");
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
        const std::array<lifetime::StartupInit, 1> instance_inputs{lifetime::glfw_instance_info(std::move(instance_info), config.present_fences)};
        lifetime::StartupArena instance_arena(instance_plan, instance_inputs);
        Vulkan<> instance(instance_arena.instance({0}));
        auto surface = AHO_NAMESPACE::window::detail::make_window_surface(window, instance);
        const std::array device_requests{storage::StartupRequest{{1}, storage::StartupKind::PHYSICAL_DEVICE},
                                         storage::StartupRequest{{2}, storage::StartupKind::DEVICE}};
        const storage::StartupPlan<2> device_plan(device_requests);
        const std::array<lifetime::StartupInit, 2> device_inputs{
            lifetime::PhysicalDeviceInit{instance, config.physical_device_index},
            lifetime::GraphicsDeviceInit{typed::ResourceId{1}, *surface, VALIDATION, config.present_fences}};
        lifetime::StartupArena device_arena(device_plan, device_inputs);
        auto device = device_arena.device({2});
        return {EngineAccessor::Create({.device = device,
                                        .commands = CommandManager(device),
                                        .descriptor_manager = std::make_shared<graphic_resource::OverAllocateManager>(
                                            device, config.descriptor_budget, 0),
                                        .synchronization = SynchroManager(device),
                                        .present_fences = config.present_fences}),
                std::move(surface)};
    }
} // namespace

namespace AHO_NAMESPACE::engine {
    // Native callbackからreceiverが破棄されても、stack側で生存を検査する。
    struct StandardEngine::BootScope {
        StandardEngine* target;
        explicit BootScope(StandardEngine& target);
        ~BootScope();
    };

    StandardEngine::BootScope::BootScope(StandardEngine& source) : target(&source) {
        if (source._boot_scope)
            throw std::logic_error("StandardEngine boot is already active");
        source._boot_scope = this;
    }

    StandardEngine::BootScope::~BootScope() {
        if (target)
            target->_boot_scope = nullptr;
    }

    StandardEngine::StandardEngine(std::string application_name, bool boot_on)
        : StandardEngine(StandardEngineConfig{.engine = {.application_name = std::move(application_name)}}, boot_on) {}

    StandardEngine::StandardEngine(StandardEngineConfig config, bool boot_on) : _config(std::move(config)) {
        validate_config(_config);
        if (boot_on)
            boot();
    }

    StandardEngine::~StandardEngine() {
        if (_boot_scope)
            _boot_scope->target = nullptr;
    }

    void StandardEngine::boot() {
        if (*this || _boot_scope)
            throw std::logic_error("StandardEngine is already booted or booting");
        BootScope scope(*this);
        const auto config = _config;
        validate_config(config);

        const auto& initial = config.initial_window;
        const auto& title = initial.title.empty() ? config.engine.application_name : initial.title;
        VSL_NAMESPACE::Window native(title, initial.width, initial.height);
        VSL_NAMESPACE::PureWindow native_snapshot = native;
        const auto os = native.window_owner();
        try {
            if (!scope.target)
                throw std::logic_error("StandardEngine was destroyed during boot");
            auto resources = create_engine_resources(config.engine, native);
            if (!scope.target)
                throw std::logic_error("StandardEngine was destroyed during resource creation");
            if (native != native_snapshot || !native_snapshot)
                throw std::runtime_error("Window closed during engine creation");

            auto handle = std::move(resources.engine);
            auto drawing = window::detail::make_window_data(handle, std::move(resources.surface));
            if (!scope.target)
                throw std::logic_error("StandardEngine was destroyed during drawing resource creation");
            if (native != native_snapshot || !native_snapshot)
                throw std::runtime_error("Window closed during drawing resource creation");

            std::optional<Window> window(std::in_place, handle, native, std::move(drawing));
            std::optional<DefaultEngineRegistration> registration;
            if (config.register_as_default)
                registration.emplace(handle);
            if (!scope.target)
                throw std::logic_error("StandardEngine was destroyed before publication");

            static_assert(std::is_nothrow_swappable_v<decltype(window)> && std::is_nothrow_swappable_v<decltype(registration)>);
            scope.target->boot_window.swap(window);
            scope.target->swap_runtime(handle);
            scope.target->_default_registration.swap(registration);

            window.reset();
            handle = {};
            registration.reset();
            if (!scope.target)
                throw std::logic_error("StandardEngine was destroyed during previous resource release");
        } catch (...) {
            native_snapshot.close();
            throw;
        }
    }

    void StandardEngine::boot(const std::string& application_name) {
        if (*this || _boot_scope)
            throw std::logic_error("StandardEngine is already booted or booting");
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
