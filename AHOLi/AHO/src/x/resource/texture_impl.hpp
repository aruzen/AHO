#pragma once
#include <AHO/x/resource/texture.hpp>
#include <stdexcept>

namespace AHO_NAMESPACE::x::resource {
    struct Texture::Data : VSL_NAMESPACE::ResourceUseState {
        VSL_NAMESPACE::Image<VSL_NAMESPACE::ImageType::SAMPLED | VSL_NAMESPACE::ImageType::TRANSFER_DST | VSL_NAMESPACE::ImageType::TRANSFER_SRC> image;
        std::uint32_t width, height;
        bool initialized = false;
        VSL_NAMESPACE::CompletionObservation update;
        Data(VSL_NAMESPACE::LogicalDeviceAccessor device, std::uint32_t width, std::uint32_t height, VSL_NAMESPACE::data_format::Format format);
        void settle();
    };

    inline Texture::Data::Data(VSL_NAMESPACE::LogicalDeviceAccessor device, std::uint32_t width, std::uint32_t height, VSL_NAMESPACE::data_format::Format format)
        : image(device, width, height, format), width(width), height(height) {}

    inline void Texture::Data::settle() {
        const auto status = update.status();
        if (status == VSL_NAMESPACE::Completion::Status::COMPLETE) {
            initialized = true;
            update = {};
        } else if (status == VSL_NAMESPACE::Completion::Status::FAILED) {
            if (update.failure() == VSL_NAMESPACE::Completion::Failure::DEVICE_LOST)
                throw std::runtime_error("Texture device was lost; recreate the texture with a new device");
            update = {};
        } else if (status == VSL_NAMESPACE::Completion::Status::CANCELLED)
            update = {};
    }
} // namespace AHO_NAMESPACE::x::resource
