#pragma once
#include <AHO/x/resource/texture.hpp>
#include <VSL/lifetime/dependency_node.hpp>
#include <stdexcept>

namespace AHO_NAMESPACE::x::resource {
    struct Texture::Data : VSL_NAMESPACE::ResourceUseState {
        VSL_NAMESPACE::lifetime::DependencyNode node;
        VSL_NAMESPACE::Image<VSL_NAMESPACE::ImageType::SAMPLED | VSL_NAMESPACE::ImageType::TRANSFER_DST | VSL_NAMESPACE::ImageType::TRANSFER_SRC> image;
        std::uint32_t width, height;
        bool initialized = false;
        VSL_NAMESPACE::CompletionObservation update;
        Data(VSL_NAMESPACE::LogicalDeviceAccessor device, std::uint32_t width, std::uint32_t height, VSL_NAMESPACE::data_format::Format format);
        ~Data() { node.retire(); }
        static void Visit(const void* context, VSL_NAMESPACE::lifetime::DependencyNode::EdgeVisitor visitor, void* output);
        void settle();
    };

    inline Texture::Data::Data(VSL_NAMESPACE::LogicalDeviceAccessor device, std::uint32_t width, std::uint32_t height, VSL_NAMESPACE::data_format::Format format)
        : ResourceUseState(VSL_NAMESPACE::UncertainUsePolicy::REQUIRE_RECREATION), image(device, width, height, format), width(width), height(height) {}

    inline void Texture::Data::Visit(const void* context, VSL_NAMESPACE::lifetime::DependencyNode::EdgeVisitor visitor, void* output) {
        visitor(output, static_cast<const Data*>(context)->image.resource_use().owner);
    }

    inline void Texture::Data::settle() {
        const auto status = update.status();
        if (status == VSL_NAMESPACE::Completion::Status::COMPLETE) {
            initialized = true;
            update = {};
        } else if (status == VSL_NAMESPACE::Completion::Status::FAILED) {
            if (update.native_uncertain())
                throw std::runtime_error("Texture state is uncertain; recreate the texture");
            if (update.failure() == VSL_NAMESPACE::Completion::Failure::DEVICE_LOST)
                throw std::runtime_error("Texture device was lost; recreate the texture with a new device");
            update = {};
        } else if (status == VSL_NAMESPACE::Completion::Status::CANCELLED)
            update = {};
    }
} // namespace AHO_NAMESPACE::x::resource
