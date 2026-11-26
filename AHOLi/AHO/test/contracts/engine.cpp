#include <AHO/engine/standard_engine.hpp>
#include <AHO/pipeline/layouts.hpp>
#include <type_traits>
#include <stdexcept>
#include <utility>

#ifndef VSL_TYPED_CASE
#define VSL_TYPED_CASE 0
#endif

#if VSL_TYPED_CASE == 0
static_assert(std::is_nothrow_move_constructible_v<aho::engine::EngineAccessor>);
static_assert(std::is_nothrow_move_assignable_v<aho::engine::EngineAccessor>);
static_assert(!std::is_base_of_v<aho::engine::EngineAccessor, aho::engine::GraphicalEngine>);
static_assert(!std::is_convertible_v<aho::StandardEngine&, aho::engine::EngineAccessor&>);

struct CustomEngine : aho::engine::GraphicalEngine {
    explicit CustomEngine(aho::engine::EngineAccessor value) { set_runtime(std::move(value)); }
    aho::Window get_window() override { throw std::logic_error("No window selected"); }
};

void connect_engine(aho::engine::EngineCreateInfo info);

void connect_engine(aho::engine::EngineCreateInfo info) {
    auto runtime = aho::engine::EngineAccessor::Create(std::move(info));
    CustomEngine engine(runtime);
    aho::engine::EngineAccessor copy = engine;
    auto moved = std::move(copy);
    runtime.swap(moved);
    (void)engine.runtime();
    (void)runtime.device();
    (void)runtime.commands();
    (void)runtime.descriptor_manager();
    (void)runtime.synchronization();
    (void)runtime.instance();
    (void)runtime.physical_device();
    (void)runtime.present_fences();
    aho::engine::DefaultEngineRegistration registration(runtime);
    (void)(aho::engine::DefaultEngineRegistration::Current() == runtime);
    (void)aho::pipeline::GET_BASE_LAYOUT(runtime);
}
#elif VSL_TYPED_CASE == 1
void reject(aho::engine::EngineAccessor value);

void reject(aho::engine::EngineAccessor value) {
    (void)value.data();
}
#elif VSL_TYPED_CASE == 2
void reject(std::shared_ptr<aho::engine::EngineData> value);

void reject(std::shared_ptr<aho::engine::EngineData> value) {
    (void)aho::engine::EngineAccessor(value);
}
#elif VSL_TYPED_CASE == 3
static_assert(sizeof(aho::engine::EngineData) > 0);
#elif VSL_TYPED_CASE == 4
void reject(aho::engine::EngineAccessor value);

void reject(aho::engine::EngineAccessor value) {
    (void)value._data;
}
#elif VSL_TYPED_CASE == 5
void reject(aho::engine::GraphicalEngine& value);

void reject(aho::engine::GraphicalEngine& value) {
    aho::engine::EngineAccessor& base = value;
    (void)base;
}
#elif VSL_TYPED_CASE == 6
void reject(aho::engine::GraphicalEngine& value);

void reject(aho::engine::GraphicalEngine& value) {
    (void)value._runtime;
}
#elif VSL_TYPED_CASE == 7
struct OldEngine : aho::engine::GraphicalEngine {
    explicit OldEngine(std::shared_ptr<aho::engine::EngineData> value) { reset_data(std::move(value)); }
};
#elif VSL_TYPED_CASE == 8
void reject(aho::engine::GraphicalEngine& value, aho::engine::EngineAccessor replacement);

void reject(aho::engine::GraphicalEngine& value, aho::engine::EngineAccessor replacement) {
    value.set_runtime(std::move(replacement));
}
#elif VSL_TYPED_CASE == 9
void reject(aho::StandardEngine& value);

void reject(aho::StandardEngine& value) {
    (void)value._boot_scope;
}
#elif VSL_TYPED_CASE == 10
void reject(aho::StandardEngine* value);

void reject(aho::StandardEngine* value) {
    (void)aho::pipeline::GET_BASE_LAYOUT(value);
}
#elif VSL_TYPED_CASE == 11
void reject(aho::engine::EngineAccessor value);

void reject(aho::engine::EngineAccessor value) {
    vsl::LogicalDeviceAccessor& borrowed = value.device();
    (void)borrowed;
}
#elif VSL_TYPED_CASE == 12
void reject(aho::engine::EngineAccessor value);

void reject(aho::engine::EngineAccessor value) {
    vsl::CommandManager& borrowed = value.commands();
    (void)borrowed;
}
#elif VSL_TYPED_CASE == 13
void reject(aho::engine::EngineAccessor value);

void reject(aho::engine::EngineAccessor value) {
    std::shared_ptr<vsl::graphic_resource::ManagerInterface>& borrowed = value.descriptor_manager();
    (void)borrowed;
}
#elif VSL_TYPED_CASE == 14
void reject(aho::engine::EngineCreateInfo value);

void reject(aho::engine::EngineCreateInfo value) {
    (void)value.vulkan_instance;
}
#endif
