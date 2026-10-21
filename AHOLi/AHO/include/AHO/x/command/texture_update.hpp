#pragma once
#include <AHO/x/resource/texture.hpp>
#include <VSL/vulkan/command.hpp>

namespace AHO_NAMESPACE::x::command {
    // 描画streamと同じ<<で記録する。生成時には転送せず、invoke時のデータを使う。
    struct TextureUpdate : VSL_NAMESPACE::command::__Command {
        void invoke(VSL_NAMESPACE::CommandPool pool, VSL_NAMESPACE::CommandBuffer buffer, VSL_NAMESPACE::CommandManager manager) override;

    private:
        friend class resource::Texture;
        TextureUpdate(resource::Texture texture, AHO_NAMESPACE::resource::BufferBinding source, VSL_NAMESPACE::ImageRegion region, std::size_t offset);
        resource::Texture _texture;
        AHO_NAMESPACE::resource::BufferBinding _source;
        VSL_NAMESPACE::ImageRegion _region;
        std::size_t _offset;
    };
} // namespace AHO_NAMESPACE::x::command

namespace AHO_NAMESPACE::x::resource {
    template<AHO_NAMESPACE::resource::GPUBuffer Source>
    AHO_NAMESPACE::x::command::TextureUpdate Texture::update(const Source& source, VSL_NAMESPACE::ImageRegion region, std::size_t offset) const {
        return update(source.binding(), region, offset);
    }
} // namespace AHO_NAMESPACE::x::resource
