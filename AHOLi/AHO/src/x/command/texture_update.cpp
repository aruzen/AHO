#include <AHO/x/command/texture_update.hpp>
#include <VSL/vulkan/commands/image_upload.hpp>
#include "../resource/texture_impl.hpp"

namespace AHO_NAMESPACE::x::command {
    TextureUpdate::TextureUpdate(resource::Texture texture, AHO_NAMESPACE::resource::BufferBinding source, VSL_NAMESPACE::ImageRegion region, std::size_t offset)
        : _texture(std::move(texture)), _source(std::move(source)), _region(region), _offset(offset) {
    }

    TextureUpdate::TextureUpdate(resource::Texture texture, AHO_NAMESPACE::resource::Buffer<AHO_NAMESPACE::resource::BufferType::GENERAL> source,
                                 VSL_NAMESPACE::ImageRegion region, std::size_t offset)
        : _texture(std::move(texture)), _source(std::move(source)), _region(region), _offset(offset) {}

    void TextureUpdate::invoke(VSL_NAMESPACE::CommandPool pool, VSL_NAMESPACE::CommandBuffer buffer, VSL_NAMESPACE::CommandManager manager) {
        const auto texture_owner = _texture._data;
        auto& texture = *texture_owner;
        const auto inputs = _source;
        const auto region = _region;
        const auto offset = _offset;
        auto completion = buffer.completion();
        if (!completion || completion.status() != VSL_NAMESPACE::Completion::Status::PENDING)
            throw std::logic_error("Texture update requires a recording phase");
        texture.settle();
        texture.image.use_state().check_available(completion);
        texture.check_available(completion);
        auto source = std::visit(
            [&](const auto& value) -> AHO_NAMESPACE::resource::BufferBinding {
                if constexpr (std::same_as<std::decay_t<decltype(value)>, AHO_NAMESPACE::resource::Buffer<AHO_NAMESPACE::resource::BufferType::GENERAL>>) {
                    AHO_NAMESPACE::resource::Buffer<AHO_NAMESPACE::resource::BufferType::GPU_FRIENDLY> staging(manager.get_device());
                    staging.assign(value.bytes());
                    return staging.binding();
                } else
                    return value;
            },
            inputs);
        const auto snapshot = source.snapshot();
        source.check_available(completion);
        auto layout = texture.initialized || texture.update ? VSL_NAMESPACE::ImageLayout::SHADER_READ_ONLY_OPTIMAL : VSL_NAMESPACE::ImageLayout::UNDEFINED;
        VSL_NAMESPACE::command::ImageUpload command(snapshot.storage(), texture.image, layout, region, offset, snapshot.constraints(), snapshot.retention());
        const auto info = command.copy.description();
        if (offset > snapshot.size() || info.byte_size > snapshot.size() - offset)
            throw std::out_of_range("Texture update exceeds initialized upload data");
        if (manager.get_device() != snapshot.device() || texture.image.get_device() != snapshot.device())
            throw std::invalid_argument("Texture update device mismatch");
        command.invoke(pool, buffer, manager);
        texture.update = VSL_NAMESPACE::CompletionObservation(completion);
        texture.mark_used(completion);
    }
} // namespace AHO_NAMESPACE::x::command

namespace AHO_NAMESPACE::x::resource {
    AHO_NAMESPACE::x::command::TextureUpdate Texture::update(const AHO_NAMESPACE::resource::Buffer<AHO_NAMESPACE::resource::BufferType::GENERAL>& source,
                                                             VSL_NAMESPACE::ImageRegion region, std::size_t offset) const {
        if (!_data)
            throw std::logic_error("Texture is empty");
        return AHO_NAMESPACE::x::command::TextureUpdate(*this, source, region, offset);
    }

    AHO_NAMESPACE::x::command::TextureUpdate Texture::update(AHO_NAMESPACE::resource::BufferBinding source, VSL_NAMESPACE::ImageRegion region, std::size_t offset) const {
        if (!_data)
            throw std::logic_error("Texture or upload source is empty");
        return AHO_NAMESPACE::x::command::TextureUpdate(*this, source, region, offset);
    }
} // namespace AHO_NAMESPACE::x::resource
