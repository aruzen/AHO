#include <VSL/vulkan/swapchain.hpp>
#include <VSL/lifetime/swapchain_arena.hpp>
#include <type_traits>

#ifndef VSL_TYPED_CASE
#define VSL_TYPED_CASE 0
#endif

static_assert(!std::is_aggregate_v<vsl::SwapchainAccessor>);
static_assert(std::is_nothrow_move_constructible_v<vsl::SwapchainAccessor>);
static_assert(std::is_nothrow_move_constructible_v<vsl::Swapchain>);
static_assert(std::is_nothrow_swappable_v<vsl::SwapchainAccessor>);
static_assert(std::is_same_v<decltype(std::declval<const vsl::SwapchainAccessor&>().images()), std::span<const vsl::ImageAccessor>>);
static_assert(std::is_same_v<decltype(std::declval<vsl::SwapchainAccessor&&>().image(0)), vsl::ImageAccessor>);

void connect_swapchain(const vsl::SwapchainAccessor& source, vsl::SwapchainAccessor value, vsl::lifetime::SwapchainArena& arena);
void connect_swapchain(const vsl::SwapchainAccessor& source, vsl::SwapchainAccessor value, vsl::lifetime::SwapchainArena& arena) {
    auto image = std::move(value).image(0);
    auto arena_image = arena.view().image(0);
    std::span<const vsl::ImageAccessor> images = source.images();
    auto copied = source;
    copied.swap(value);
    (void)bool(copied);
    (void)copied.identity();
    (void)copied.get_swap_image_size();
    (void)copied.owner_ref();
    (void)copied.get_device();
    (void)copied.get_format();
    (void)image;
    (void)arena_image;
    (void)images;
}

#if VSL_TYPED_CASE == 1
void rejected(vsl::SwapchainAccessor value);
void rejected(vsl::SwapchainAccessor value) {
    (void)value.data_;
}
#elif VSL_TYPED_CASE == 2
void rejected(vsl::SwapchainAccessor value);
void rejected(vsl::SwapchainAccessor value) {
    (void)value._owner;
}
#elif VSL_TYPED_CASE == 3
void rejected(vsl::SwapchainAccessor value);
void rejected(vsl::SwapchainAccessor value) {
    (void)value._images;
}
#elif VSL_TYPED_CASE == 4
void rejected(std::shared_ptr<vsl::impl::SwapchainImpl> owner, std::vector<vsl::ImageAccessor> images);
void rejected(std::shared_ptr<vsl::impl::SwapchainImpl> owner, std::vector<vsl::ImageAccessor> images) {
    (void)vsl::SwapchainAccessor(std::move(owner), std::move(images));
}
#elif VSL_TYPED_CASE == 5
void rejected(vsl::lifetime::OwnedRef<vsl::impl::SwapchainImpl> owner, std::vector<vsl::ImageAccessor> images);
void rejected(vsl::lifetime::OwnedRef<vsl::impl::SwapchainImpl> owner, std::vector<vsl::ImageAccessor> images) {
    (void)vsl::SwapchainAccessor(std::move(owner), std::move(images));
}
#elif VSL_TYPED_CASE == 6
void rejected(vsl::SwapchainAccessor value);
void rejected(vsl::SwapchainAccessor value) {
    (void)std::move(value).images();
}
#elif VSL_TYPED_CASE == 7
void rejected(const vsl::SwapchainAccessor value);
void rejected(const vsl::SwapchainAccessor value) {
    (void)std::move(value).images();
}
#elif VSL_TYPED_CASE == 8
void rejected(vsl::SwapchainAccessor value);
void rejected(vsl::SwapchainAccessor value) {
    value.images()[0] = vsl::ImageAccessor{};
}
#elif VSL_TYPED_CASE == 9
void rejected(vsl::SwapchainIdentity chain, vsl::BufferIdentity buffer);
void rejected(vsl::SwapchainIdentity chain, vsl::BufferIdentity buffer) {
    (void)(chain == buffer);
}
#elif VSL_TYPED_CASE == 10
void rejected(vsl::SwapchainAccessor value);
void rejected(vsl::SwapchainAccessor value) {
    (void)value.identity().get();
}
#endif
