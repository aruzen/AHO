#pragma once
#include <AHO/resource/buffer.hpp>
#include <VSL/vulkan/image.hpp>

namespace AHO_NAMESPACE::x::command {
    struct TextureUpdate;
}

namespace AHO_NAMESPACE::x::resource {
    class Texture {
        struct Data;
        std::shared_ptr<Data> _data;
        friend struct AHO_NAMESPACE::x::command::TextureUpdate;

    public:
        Texture(VSL_NAMESPACE::LogicalDeviceAccessor device, std::uint32_t width, std::uint32_t height,
                VSL_NAMESPACE::data_format::Format format = VSL_NAMESPACE::data_format::UNSIGNED_NORMALIZED8_RGBA);
        std::uint32_t width() const;
        std::uint32_t height() const;
        VSL_NAMESPACE::data_format::Format format() const;
        VSL_NAMESPACE::ImageAccessor image() const;
        VSL_NAMESPACE::ResourceUse resource_use() const;
        // レンダーパス外で記録する。初回は未更新の画素もゼロになる。
        AHO_NAMESPACE::x::command::TextureUpdate update(AHO_NAMESPACE::resource::BufferBinding source, VSL_NAMESPACE::ImageRegion region = {}, std::size_t offset = 0) const;
        template<AHO_NAMESPACE::resource::gpu_buffer Source>
        AHO_NAMESPACE::x::command::TextureUpdate update(const Source& source, VSL_NAMESPACE::ImageRegion region = {}, std::size_t offset = 0) const;
    };

} // namespace AHO_NAMESPACE::x::resource
