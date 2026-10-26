#include <AHO/x/resource/texture.hpp>
#include "texture_impl.hpp"
#include <stdexcept>

namespace AHO_NAMESPACE::x::resource {
    Texture::Texture(VSL_NAMESPACE::LogicalDeviceAccessor device, std::uint32_t width, std::uint32_t height, VSL_NAMESPACE::data_format::Format format) {
        // アトラス用の線形RGBA8に絞る。整数形式やsRGBの暗黙変換は行わない。
        if (!device.data_ || !width || !height || format != VSL_NAMESPACE::data_format::UNSIGNED_NORMALIZED8_RGBA)
            throw std::invalid_argument("Texture prototype requires a device, nonzero extent and linear RGBA8");
        _data = std::make_shared<Data>(device, width, height, format);
    }

    std::uint32_t Texture::width() const {
        return _data ? _data->width : 0;
    }

    std::uint32_t Texture::height() const {
        return _data ? _data->height : 0;
    }

    VSL_NAMESPACE::data_format::Format Texture::format() const {
        if (!_data)
            throw std::logic_error("Texture is empty");
        return _data->image.get_format();
    }

    VSL_NAMESPACE::ImageAccessor Texture::image() const {
        if (!_data)
            throw std::logic_error("Texture is empty");
        return _data->image;
    }
    VSL_NAMESPACE::ResourceUse Texture::resource_use() const {
        if (!_data)
            throw std::logic_error("Texture is empty");
        return {_data, _data->image.data_};
    }
} // namespace AHO_NAMESPACE::x::resource
