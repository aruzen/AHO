#include <AHO/x/resource/texture.hpp>
#include <VSL/vulkan/commands/image_upload.hpp>
#include <stdexcept>

namespace AHO_NAMESPACE::x::resource {
    struct Texture::Data {
        VSL_NAMESPACE::Image<VSL_NAMESPACE::ImageType::Sampled | VSL_NAMESPACE::ImageType::TransferDst | VSL_NAMESPACE::ImageType::TransferSrc> image;
        std::uint32_t width, height;
        bool initialized = false;
        VSL_NAMESPACE::Completion update;
        Data(VSL_NAMESPACE::LogicalDeviceAccessor device, std::uint32_t width, std::uint32_t height, VSL_NAMESPACE::data_format::___Format format)
            : image(device, width, height, format), width(width), height(height) {}
        void settle() {
            const auto status = update.status();
            if (status == VSL_NAMESPACE::Completion::Status::Complete) {
                initialized = true;
                update = {};
            } else if (status == VSL_NAMESPACE::Completion::Status::Cancelled)
                update = {};
        }
    };
    Texture::Texture(VSL_NAMESPACE::LogicalDeviceAccessor device, std::uint32_t width, std::uint32_t height, VSL_NAMESPACE::data_format::___Format format) {
        // アトラス用の線形RGBA8に絞る。整数形式やsRGBの暗黙変換は行わない。
        if (!device._data || !width || !height || format != VSL_NAMESPACE::data_format::UnsignedNormalized8RGBA)
            throw std::invalid_argument("Texture prototype requires a device, nonzero extent and linear RGBA8");
        _data = std::make_shared<Data>(device, width, height, format);
    }
    std::uint32_t Texture::width() const { return _data ? _data->width : 0; }
    std::uint32_t Texture::height() const { return _data ? _data->height : 0; }
    VSL_NAMESPACE::data_format::___Format Texture::format() const {
        if (!_data)
            throw std::logic_error("Texture is empty");
        return _data->image.getFormat();
    }
    VSL_NAMESPACE::ImageAccessor Texture::image() const {
        if (!_data)
            throw std::logic_error("Texture is empty");
        return _data->image;
    }
    TextureUpdate Texture::update(AHO_NAMESPACE::resource::BufferBinding source, VSL_NAMESPACE::ImageRegion region, std::size_t offset) const {
        if (!_data)
            throw std::logic_error("Texture or upload source is empty");
        return TextureUpdate(*this, source, region, offset);
    }
    TextureUpdate::TextureUpdate(Texture texture, AHO_NAMESPACE::resource::BufferBinding source, VSL_NAMESPACE::ImageRegion region, std::size_t offset)
        : texture_(std::move(texture)), source_(std::move(source)), region_(region), offset_(offset) {}
    void TextureUpdate::invoke(VSL_NAMESPACE::CommandPool pool, VSL_NAMESPACE::CommandBuffer buffer, VSL_NAMESPACE::CommandManager manager) {
        auto& texture = *texture_._data;
        const auto& source = source_;
        auto completion = buffer.completion();
        if (!completion || completion.status() != VSL_NAMESPACE::Completion::Status::Pending)
            throw std::logic_error("Texture update requires a recording phase");
        texture.settle();
        source.checkAvailable(completion);
        if (texture.update && texture.update != completion)
            throw std::logic_error("Texture or upload source is still in use by another phase");
        auto layout = texture.initialized || texture.update ? VSL_NAMESPACE::ImageLayout::ShaderReadOnlyOptimal : VSL_NAMESPACE::ImageLayout::Undefined;
        VSL_NAMESPACE::command::ImageUpload command(&source.native(), texture.image, layout, region_, offset_);
        const auto info = command.copy.description();
        if (offset_ > source.size() || info.byteSize > source.size() - offset_)
            throw std::out_of_range("Texture update exceeds initialized upload data");
        if (manager.getDevice() != source.device() || texture.image.getDevice() != source.device())
            throw std::invalid_argument("Texture update device mismatch");
        buffer.retain(source.owner());
        command.invoke(pool, buffer, manager);
        texture.update = completion;
        source.markUsed(std::move(completion));
    }
} // namespace AHO_NAMESPACE::x::resource
