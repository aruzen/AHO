#pragma once
#include <VSL/vulkan/image.hpp>
#include <VSL/operation/image_create_info.hpp>

namespace VSL_NAMESPACE::lifetime::detail {
    struct ImageAccess {
        static const OwnedRef<impl::ImageImpl>& Owned(const ImageAccessor& image) { return image._owner; }
        static OwnedRef<impl::ImageImpl>& Owned(ImageAccessor& image) { return image._owner; }
        static OwnedRef<impl::ImageImpl> Owned(ImageAccessor&& image) { return std::move(image._owner); }
        static OwnedRef<impl::ImageImpl> Owned(const ImageAccessor&& image) { return image._owner; }
        static ImageAccessor FromOwned(OwnedRef<impl::ImageImpl> owner) { return ImageAccessor(std::move(owner)); }
        static ImageAccessor FromShared(std::shared_ptr<impl::ImageImpl> owner);
    };
} // namespace VSL_NAMESPACE::lifetime::detail
