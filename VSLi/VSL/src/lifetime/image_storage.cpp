#include <VSL/lifetime/image_storage.hpp>
#include <new>

namespace VSL_NAMESPACE::lifetime {
    static_assert(std::is_nothrow_default_constructible_v<impl::ImageImpl>);
    namespace {
        struct HeapImageOwner {
            LogicalDeviceAccessor device;
            std::shared_ptr<impl::SwapchainImpl> swapchain;
            alignas(ImageSlotStorage) std::byte storage[sizeof(ImageSlotStorage)];
            HeapImageOwner(LogicalDeviceAccessor device, const ImageCreateInfo& info,
                           std::shared_ptr<impl::SwapchainImpl> swapchain = {}, vk::Image borrowed = {});
            ~HeapImageOwner();
        };
        HeapImageOwner::HeapImageOwner(LogicalDeviceAccessor device, const ImageCreateInfo& info,
                                       std::shared_ptr<impl::SwapchainImpl> swapchain, vk::Image borrowed)
            : device(device), swapchain(std::move(swapchain)) {
            construct_image(storage, device, info, borrowed);
        }
        HeapImageOwner::~HeapImageOwner() {
            destroy_image(storage);
        }
    } // namespace

    void construct_image(void* storage, LogicalDeviceAccessor device, const ImageCreateInfo& info, vk::Image borrowed) {
        if (!device.data_ || !device.data_->device)
            throw std::invalid_argument("Image requires a live device");
        auto* slot = std::construct_at(static_cast<ImageSlotStorage*>(storage));
        auto* image = std::construct_at(reinterpret_cast<impl::ImageImpl*>(slot->image));
        image->use = std::construct_at(reinterpret_cast<ResourceUseState*>(slot->use));
        try {
            if (borrowed)
                operation::initialize_borrowed_image(*image, *device.data_, info, borrowed);
            else
                operation::initialize_image(*image, *device.data_, info);
        } catch (...) {
            std::destroy_at(image->use);
            std::destroy_at(image);
            std::destroy_at(slot);
            throw;
        }
    }
    impl::ImageImpl& image_at(void* storage) {
        auto* slot = static_cast<ImageSlotStorage*>(storage);
        return *std::launder(reinterpret_cast<impl::ImageImpl*>(slot->image));
    }
    void destroy_image(void* storage) noexcept {
        auto& image = image_at(storage);
        operation::destroy_image(image);
        std::destroy_at(image.use);
        std::destroy_at(&image);
        std::destroy_at(static_cast<ImageSlotStorage*>(storage));
    }
    std::shared_ptr<impl::ImageImpl> make_heap_image(LogicalDeviceAccessor device, const ImageCreateInfo& info) {
        auto owner = std::make_shared<HeapImageOwner>(device, info);
        auto* image = &image_at(owner->storage);
        return {std::move(owner), image};
    }
    std::shared_ptr<impl::ImageImpl> make_swapchain_image(LogicalDeviceAccessor device, const ImageCreateInfo& info,
                                                          std::shared_ptr<impl::SwapchainImpl> swapchain, vk::Image image) {
        if (!swapchain || !image || swapchain->device != device.data_)
            throw std::invalid_argument("Borrowed swapchain image requires its owning swapchain and device");
        auto owner = std::make_shared<HeapImageOwner>(device, info, std::move(swapchain), image);
        auto* result = &image_at(owner->storage);
        return {std::move(owner), result};
    }
} // namespace VSL_NAMESPACE::lifetime
