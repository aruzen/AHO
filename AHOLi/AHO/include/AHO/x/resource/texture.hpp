#pragma once
#include <AHO/resource/buffer.hpp>
#include <VSL/vulkan/image.hpp>
#include <VSL/vulkan/phase.hpp>

namespace AHO_NAMESPACE::x::resource {
    struct TextureUpdate;
    class Texture {
        struct Data;
        std::shared_ptr<Data> _data;
        friend struct TextureUpdate;

    public:
        Texture(VSL_NAMESPACE::LogicalDeviceAccessor device, std::uint32_t width, std::uint32_t height,
                VSL_NAMESPACE::data_format::___Format format = VSL_NAMESPACE::data_format::UnsignedNormalized8RGBA);
        std::uint32_t width() const;
        std::uint32_t height() const;
        VSL_NAMESPACE::data_format::___Format format() const;
        VSL_NAMESPACE::ImageAccessor image() const;
        // レンダーパス外で記録する。初回は未更新の画素もゼロになる。
        TextureUpdate update(AHO_NAMESPACE::resource::BufferBinding source, VSL_NAMESPACE::ImageRegion region = {}, std::size_t offset = 0) const;
        template<AHO_NAMESPACE::resource::GPUBuffer Source>
        TextureUpdate update(const Source& source, VSL_NAMESPACE::ImageRegion region = {}, std::size_t offset = 0) const;
    };

    // 描画streamと同じ<<で記録する。生成時には転送せず、invoke時のデータを使う。
    struct TextureUpdate : VSL_NAMESPACE::command::__Command {
        void invoke(VSL_NAMESPACE::CommandPool pool, VSL_NAMESPACE::CommandBuffer buffer, VSL_NAMESPACE::CommandManager manager) override;

    private:
        friend class Texture;
        TextureUpdate(Texture texture, AHO_NAMESPACE::resource::BufferBinding source, VSL_NAMESPACE::ImageRegion region, std::size_t offset);
        Texture _texture;
        AHO_NAMESPACE::resource::BufferBinding _source;
        VSL_NAMESPACE::ImageRegion _region;
        std::size_t _offset;
    };

    template<AHO_NAMESPACE::resource::GPUBuffer Source>
    TextureUpdate Texture::update(const Source& source, VSL_NAMESPACE::ImageRegion region, std::size_t offset) const {
        return update(source.binding(), region, offset);
    }
} // namespace AHO_NAMESPACE::x::resource
