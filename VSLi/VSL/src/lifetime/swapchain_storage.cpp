#include <VSL/lifetime/swapchain_storage.hpp>
#include <VSL/lifetime/swapchain_arena.hpp>
#include <VSL/lifetime/device_owner.hpp>
#include <VSL/lifetime/surface_storage.hpp>
#include <VSL/lifetime/image_storage.hpp>
#include <VSL/lifetime/glfw_lease.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <VSL/lifetime/presentation_retention.hpp>
#include <VSL/lifetime/synchronization_storage.hpp>
#include <VSL/storage/cpu_region.hpp>
#include <new>

namespace VSL_NAMESPACE::lifetime {
    static_assert(std::is_standard_layout_v<SwapchainSlotStorage> && offsetof(SwapchainSlotStorage, value) == 0);
    static_assert(std::is_nothrow_default_constructible_v<impl::SwapchainImpl>);
    namespace {
        struct SwapchainGenerationOwner {
            DependencyNode node;
            std::array<OwnerRef, 2> parents;
            size_t parent_count = 0;
            std::shared_ptr<storage::detail::CPURegion> memory;
            storage::SwapchainLayout layout;
            SwapchainPresentationContext presentation;
            size_t images_constructed = 0;
            bool claimed = false, chain_constructed = false, fences_constructed = false;
            SwapchainGenerationOwner(operation::PendingSwapchain& pending, storage::SwapchainLayout layout,
                                     std::shared_ptr<storage::detail::CPURegion> memory, std::array<OwnerRef, 2> parents, size_t parent_count,
                                     const operation::ImageViewNativeCalls& image_calls);
            ~SwapchainGenerationOwner();
            void* image_slot(size_t index) const;
            void clear() noexcept;
            static void VisitDependencies(const void* owner, DependencyNode::EdgeVisitor visitor, void* context);
        };
        SwapchainGenerationOwner::SwapchainGenerationOwner(operation::PendingSwapchain& pending, storage::SwapchainLayout layout,
                                                           std::shared_ptr<storage::detail::CPURegion> memory, std::array<OwnerRef, 2> parents,
                                                           size_t parent_count, const operation::ImageViewNativeCalls& image_calls)
            : parents(std::move(parents)), parent_count(parent_count), memory(std::move(memory)), layout(layout), presentation(layout.images()) {
            if (!this->memory)
                throw std::invalid_argument("Swapchain requires CPU memory");
            layout.require_region(this->memory->bytes, this->memory->alignment);
            this->memory->claim(layout.bytes(), layout.alignment());
            claimed = true;
            try {
                auto format = data_format::convert(static_cast<data_format::FormatTypes>(pending.format.format));
                if (format.type() == data_format::FormatTypes::UNDEFINED)
                    format = data_format::Format(static_cast<data_format::FormatTypes>(pending.format.format), 0);
                const ImageCreateInfo info{pending.usage, pending.extent.width, pending.extent.height, format};
                for (size_t i = 0; i < pending.images.size(); ++i) {
                    construct_image(image_slot(i), *pending.device, info, pending.images[i], image_calls);
                    ++images_constructed;
                }
                if (layout.has_present_fences()) {
                    auto* slot = this->memory->base + layout.present_fence_offset();
                    construct_fence(slot, *pending.device, {.count = layout.images()});
                    fences_constructed = true;
                    presentation.fences = &fence_at(slot);
                }
                construct_swapchain(this->memory->base, pending);
                chain_constructed = true;
            } catch (...) {
                clear();
                throw;
            }
        }
        SwapchainGenerationOwner::~SwapchainGenerationOwner() {
            node.retire();
            clear();
        }
        void* SwapchainGenerationOwner::image_slot(size_t index) const {
            return memory->base + layout.image_offset(index);
        }
        void SwapchainGenerationOwner::clear() noexcept {
            if (fences_constructed) {
                destroy_fence(memory->base + layout.present_fence_offset());
                fences_constructed = false;
            }
            while (images_constructed)
                destroy_image(image_slot(--images_constructed));
            if (chain_constructed) {
                destroy_swapchain(memory->base);
                chain_constructed = false;
            }
            if (claimed) {
                memory->release();
                claimed = false;
            }
        }
        void SwapchainGenerationOwner::VisitDependencies(const void* owner, DependencyNode::EdgeVisitor visitor, void* context) {
            const auto& root = *static_cast<const SwapchainGenerationOwner*>(owner);
            for (size_t i = 0; i < root.parent_count; ++i)
                visitor(context, root.parents[i]);
        }
    } // namespace
    void construct_swapchain(void* storage, operation::PendingSwapchain& pending) noexcept {
        auto* slot = std::construct_at(static_cast<SwapchainSlotStorage*>(storage));
        auto* value = std::construct_at(reinterpret_cast<impl::SwapchainImpl*>(slot->value));
        operation::adopt_swapchain(*value, pending);
    }
    impl::SwapchainImpl& swapchain_at(void* storage) {
        return *std::launder(reinterpret_cast<impl::SwapchainImpl*>(static_cast<SwapchainSlotStorage*>(storage)->value));
    }
    void destroy_swapchain(void* storage) noexcept {
        auto& value = swapchain_at(storage);
        std::destroy_at(&value);
        std::destroy_at(static_cast<SwapchainSlotStorage*>(storage));
    }
    void require_swapchain_owner(const SwapchainAccessor& source) {
        require_swapchain_owner(source.data_);
    }
    void require_swapchain_owner(const std::shared_ptr<impl::SwapchainImpl>& value) {
        if (!value || !value.use_count() || !value->swap_chain || !value->owner_node || value->owner_node->retiring() || !value->owner_node->matches(value))
            throw std::invalid_argument("Swapchain requires its live canonical owner");
    }
    std::shared_ptr<impl::ImageImpl> swapchain_image(const std::shared_ptr<impl::SwapchainImpl>& source, size_t index) {
        require_swapchain_owner(source);
        if (!source->image_storage || !source->image_stride || index >= source->swap_chain_images.size())
            throw std::out_of_range("Swapchain image slot is unavailable");
        auto* image = std::launder(reinterpret_cast<impl::ImageImpl*>(source->image_storage + index * source->image_stride));
        return {source, image};
    }
    SwapchainAccessor make_swapchain(LogicalDeviceAccessor device, Surface surface, SwapchainCreateInfo info,
                                     const std::optional<SwapchainAccessor>& old, const SwapchainRegionFactory& memory,
                                     const operation::SwapchainNativeCalls& calls, const operation::ImageViewNativeCalls& image_calls) {
        require_device_owner(device);
        require_surface_owner(surface);
        if (!memory)
            throw std::invalid_argument("Swapchain requires a CPU storage factory");
        const auto device_owner = device.data_->owner_node->ref(std::move(device.data_));
        const auto surface_owner = surface.data_->owner_node->ref(std::move(surface.data_));
        std::shared_ptr<impl::SwapchainImpl> previous;
        if (old) {
            require_swapchain_owner(*old);
            previous = old->data_;
        }
        GLFWCallScope scope;
        if (!info.width || !info.height) {
            int width = 0, height = 0;
            glfwGetFramebufferSize(static_cast<GLFWwindow*>(surface_owner->window), &width, &height);
            if (width < 0 || height < 0)
                throw std::runtime_error("Swapchain framebuffer extent is negative");
            if (!info.width)
                info.width = static_cast<std::uint32_t>(width);
            if (!info.height)
                info.height = static_cast<std::uint32_t>(height);
        }
        // device/surface ownerはPendingより先に作り、native破棄が済むまで保持する。
        auto pending = operation::prepare_swapchain(*device_owner, *surface_owner, info, previous.get(), calls);
        const storage::SwapchainLayout layout(pending.images.size(), pending.present_fences);
        auto region = memory(layout);
        std::vector<ImageAccessor> images(layout.images());
        std::array<OwnerRef, 2> parents;
        size_t count = 0;
        detail::retain_owner(parents, count, device_owner);
        detail::retain_owner(parents, count, surface_owner);
        auto root = std::make_shared<SwapchainGenerationOwner>(pending, layout, std::move(region), std::move(parents), count, image_calls);
        auto& value = swapchain_at(root->memory->base);
        value.image_storage = root->memory->base + layout.image_offset(0);
        value.image_stride = sizeof(ImageSlotStorage);
        value.presentation_context = &root->presentation;
        root->presentation.generation = &value;
        root->node.publish(root, root.get(), SwapchainGenerationOwner::VisitDependencies);
        value.owner_node = &root->node;
        if (root->presentation.fences)
            root->presentation.fences->owner_node = &root->node;
        for (size_t i = 0; i < images.size(); ++i) {
            auto& image = image_at(root->image_slot(i));
            image.owner_node = &root->node;
            images[i].data_ = {root, &image};
        }
        return {{root, &value}, std::move(images)};
    }
    SwapchainArena::SwapchainArena(LogicalDeviceAccessor device, Surface surface, SwapchainCreateInfo info,
                                   std::optional<SwapchainAccessor> old, SwapchainMemoryFactory memory, ValidatedStorage) {
        _view = make_swapchain(std::move(device), std::move(surface), std::move(info), old, [&](const auto& layout) {
            auto region = memory ? memory(layout) : storage::ArenaMemory(layout.bytes(), layout.alignment());
            layout.require_region(region.size(), region.alignment());
            return region._region;
        });
    }
    OwnerRef SwapchainArena::owner_ref() const {
        return _view.owner_ref();
    }
    size_t SwapchainArena::image_count() const {
        return _view.images.size();
    }
} // namespace VSL_NAMESPACE::lifetime
