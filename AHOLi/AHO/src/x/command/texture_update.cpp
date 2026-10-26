#include <AHO/x/command/texture_update.hpp>
#include <VSL/vulkan/commands/image_upload.hpp>
#include "../resource/texture_impl.hpp"

namespace AHO_NAMESPACE::x::command {
    TextureUpdate::TextureUpdate(resource::Texture texture, AHO_NAMESPACE::resource::BufferBinding source, VSL_NAMESPACE::ImageRegion region, std::size_t offset)
        : _texture(std::move(texture)), _source(std::move(source)), _region(region), _offset(offset) {
    }

    void TextureUpdate::invoke(VSL_NAMESPACE::CommandPool pool, VSL_NAMESPACE::CommandBuffer buffer, VSL_NAMESPACE::CommandManager manager) {
        auto& texture = *_texture._data;
        const auto& source = _source;
        auto completion = buffer.completion();
        if (!completion || completion.status() != VSL_NAMESPACE::Completion::Status::PENDING)
            throw std::logic_error("Texture update requires a recording phase");
        texture.settle();
        source.check_available(completion);
        if (texture.update && texture.update != completion)
            throw std::logic_error("Texture or upload source is still in use by another phase");
        auto layout = texture.initialized || texture.update ? VSL_NAMESPACE::ImageLayout::SHADER_READ_ONLY_OPTIMAL : VSL_NAMESPACE::ImageLayout::UNDEFINED;
        VSL_NAMESPACE::command::ImageUpload command(&source.native(), texture.image, layout, _region, _offset);
        const auto info = command.copy.description();
        if (_offset > source.size() || info.byte_size > source.size() - _offset)
            throw std::out_of_range("Texture update exceeds initialized upload data");
        if (manager.get_device() != source.device() || texture.image.get_device() != source.device())
            throw std::invalid_argument("Texture update device mismatch");
        buffer.retain(source.owner());
        command.invoke(pool, buffer, manager);
        texture.update = completion;
        source.mark_used(std::move(completion));
    }
} // namespace AHO_NAMESPACE::x::command

namespace AHO_NAMESPACE::x::resource {
    AHO_NAMESPACE::x::command::TextureUpdate Texture::update(AHO_NAMESPACE::resource::BufferBinding source, VSL_NAMESPACE::ImageRegion region, std::size_t offset) const {
        if (!_data)
            throw std::logic_error("Texture or upload source is empty");
        return AHO_NAMESPACE::x::command::TextureUpdate(*this, source, region, offset);
    }
} // namespace AHO_NAMESPACE::x::resource
