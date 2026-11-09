#include <VSL/vulkan/descriptor_set_manager.hpp>
#include <VSL/lifetime/buffer_view.hpp>
#include <type_traits>

#ifndef VSL_TYPED_CASE
#define VSL_TYPED_CASE 0
#endif

namespace gr = vsl::graphic_resource;
static_assert(!std::is_aggregate_v<gr::BindingLayout>);
static_assert(!std::is_aggregate_v<gr::Pool>);
static_assert(!std::is_aggregate_v<gr::Resource>);
static_assert(std::is_default_constructible_v<gr::Pool>);
static_assert(std::is_default_constructible_v<gr::Resource>);
static_assert(std::is_copy_constructible_v<gr::BindingLayout>);
static_assert(std::is_nothrow_move_constructible_v<gr::Resource>);
static_assert(std::is_move_assignable_v<gr::ExactAllocateManager>);
static_assert(std::is_nothrow_move_assignable_v<gr::OverAllocateManager>);
static_assert(!std::is_assignable_v<gr::ManagerInterface&, const gr::ManagerInterface&>);
static_assert(!std::is_assignable_v<gr::ManagerInterface&, gr::ManagerInterface&&>);

struct CustomManager final : gr::ManagerInterface {
    explicit CustomManager(vsl::LogicalDeviceAccessor device) : ManagerInterface(std::move(device)) {}
    ~CustomManager() override { retire_identity(); }
    std::tuple<std::vector<gr::Pool>, std::vector<gr::Resource>> allocate(std::vector<gr::BindingLayout> layouts) override {
        auto [pool, resources] = make(std::move(layouts));
        return {{std::move(pool)}, std::move(resources)};
    }
    void destroy(const gr::Pool& pool) override { destroy_pool(pool); }
    void swap(CustomManager& other) noexcept { swap_state(other); }
};

void connect_descriptor(vsl::LogicalDeviceAccessor device, gr::Pool pool, gr::Resource resource, gr::BindingLayout layout,
                        gr::ExactAllocateManager& manager, const vsl::BufferView& buffer);
void connect_descriptor(vsl::LogicalDeviceAccessor device, gr::Pool pool, gr::Resource resource, gr::BindingLayout layout,
                        gr::ExactAllocateManager& manager, const vsl::BufferView& buffer) {
    CustomManager custom(device);
    CustomManager other(device);
    custom.swap(other);
    (void)static_cast<bool>(pool);
    (void)static_cast<bool>(resource);
    (void)static_cast<bool>(layout);
    (void)static_cast<bool>(manager);
    (void)pool.owner_ref();
    (void)layout.owner_ref();
    (void)resource.owner_ref();
    (void)pool.get_device();
    (void)layout.get_device();
    (void)resource.get_device();
    (void)manager.get_device();
    resource.update(buffer);
    auto kept = resource.get_pool();
    pool.swap(kept);
    auto copy = layout;
    layout.swap(copy);
    (void)pool.bind(layout);
    custom.destroy(pool);
}

#if VSL_TYPED_CASE == 1
void rejected(gr::BindingLayout value);
void rejected(gr::BindingLayout value) {
    (void)value.data_;
}
#elif VSL_TYPED_CASE == 2
void rejected(gr::BindingLayout value);
void rejected(gr::BindingLayout value) {
    (void)value._owner;
}
#elif VSL_TYPED_CASE == 3
void rejected(std::shared_ptr<vsl::impl::ResourceBindingLayoutImpl> value);
void rejected(std::shared_ptr<vsl::impl::ResourceBindingLayoutImpl> value) {
    (void)gr::BindingLayout(std::move(value));
}
#elif VSL_TYPED_CASE == 4
void rejected(gr::Pool value);
void rejected(gr::Pool value) {
    (void)value.data_;
}
#elif VSL_TYPED_CASE == 5
void rejected(gr::Pool value);
void rejected(gr::Pool value) {
    (void)value._owner;
}
#elif VSL_TYPED_CASE == 6
void rejected(std::shared_ptr<vsl::impl::GraphicResourcePoolImpl> value);
void rejected(std::shared_ptr<vsl::impl::GraphicResourcePoolImpl> value) {
    (void)gr::Pool(std::move(value));
}
#elif VSL_TYPED_CASE == 7
void rejected(gr::Resource value);
void rejected(gr::Resource value) {
    (void)value.data_;
}
#elif VSL_TYPED_CASE == 8
void rejected(gr::Resource value);
void rejected(gr::Resource value) {
    (void)value._owner;
}
#elif VSL_TYPED_CASE == 9
void rejected(std::shared_ptr<vsl::impl::GraphicResourceImpl> value);
void rejected(std::shared_ptr<vsl::impl::GraphicResourceImpl> value) {
    (void)gr::Resource(std::move(value));
}
#elif VSL_TYPED_CASE == 10
void rejected(gr::ManagerInterface& value);
void rejected(gr::ManagerInterface& value) {
    (void)value.data_;
}
#elif VSL_TYPED_CASE == 11
void rejected(gr::ManagerInterface& value);
void rejected(gr::ManagerInterface& value) {
    (void)value._state;
}
#elif VSL_TYPED_CASE == 12
void rejected(gr::ManagerInterface& value);
void rejected(gr::ManagerInterface& value) {
    (void)value._identity;
}
#elif VSL_TYPED_CASE == 13
void rejected(gr::ManagerInterface& value, const gr::ManagerInterface& source);
void rejected(gr::ManagerInterface& value, const gr::ManagerInterface& source) {
    value = source;
}
#elif VSL_TYPED_CASE == 14
void rejected(gr::ManagerInterface& value, gr::ManagerInterface&& source);
void rejected(gr::ManagerInterface& value, gr::ManagerInterface&& source) {
    value = std::move(source);
}
#elif VSL_TYPED_CASE == 15
void rejected(gr::ManagerInterface& value, gr::ManagerInterface& source);
void rejected(gr::ManagerInterface& value, gr::ManagerInterface& source) {
    value.swap_state(source);
}
#elif VSL_TYPED_CASE == 16
void rejected(gr::ManagerInterface& value);
void rejected(gr::ManagerInterface& value) {
    value.retire_identity();
}
#elif VSL_TYPED_CASE == 17
struct RejectedManager : gr::ManagerInterface {
    void rejected() { _state.reset(); }
};
#elif VSL_TYPED_CASE == 18
void rejected(std::shared_ptr<void> owner, vsl::impl::GraphicResourceImpl* resource);
void rejected(std::shared_ptr<void> owner, vsl::impl::GraphicResourceImpl* resource) {
    (void)gr::Resource{{owner, resource}};
}
#endif
