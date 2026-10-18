#include <AHO/image_readback.hpp>
#include <VSL/vulkan/commands/image_readback.hpp>
#include <VSL/vulkan/phase.hpp>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace AHO_NAMESPACE {
    struct ImageReadback::Data {
        vsl::CommandManager manager;
        std::optional<vsl::MappedBuffer> buffer;
        vsl::command::ImageCopyInfo info;
        vsl::Completion completion;
        explicit Data(vsl::CommandManager producer) : manager(producer.independent()) {}
        void reserve(std::size_t bytes) {
            if (bytes && (!buffer || buffer->size() < bytes)) {
                auto next = vsl::MappedBuffer(manager.getDevice(), bytes);
                buffer = std::move(next);
            }
        }
    };
    ImageReadback::ImageReadback(vsl::CommandManager manager, std::size_t initialCapacity) {
        auto data = std::make_shared<Data>(std::move(manager));
        data->reserve(initialCapacity);
        _data = std::move(data);
    }
    ImageReadback::ImageReadback(vsl::CommandManager manager, vsl::ImageAccessor image,
                                 vsl::ImageLayout layout, vsl::ImageRegion region)
        : ImageReadback(std::move(manager)) { capture(std::move(image), layout, region); }
    bool ImageReadback::capture(vsl::ImageAccessor image, vsl::ImageLayout layout, vsl::ImageRegion region) {
        if (!_data)
            throw std::logic_error("Readback handle is empty");
        const auto info = vsl::command::ImageReadback::describe(image, layout, region);
        auto &data = *_data;
        if (image.getDevice() != data.manager.getDevice())
            throw std::invalid_argument("Readback requires an image on the manager's device");
        if (data.completion && !data.completion.ready())
            return false;
        data.reserve(info.byteSize);
        auto phase = data.manager.startPhase<vsl::ComputePhase>();
        phase << vsl::command::ImageReadback(*data.buffer, image, layout, info.region);
        if (phase.submit() != vsl::ComputePhase::SubmitResult::Success)
            throw std::runtime_error("Readback submission failed");
        data.info = info;
        data.completion = phase.completion();
        return true;
    }
    bool ImageReadback::ready() const { return _data && _data->completion.ready(); }
    bool ImageReadback::wait(std::uint64_t timeout) const {
        if (!_data || !_data->completion)
            throw std::logic_error("Readback has no submitted capture");
        return _data->completion.wait(timeout);
    }
    ImagePixels ImageReadback::read(std::uint64_t timeout) const {
        ImagePixels result;
        read(result, timeout);
        return result;
    }
    void ImageReadback::read(ImagePixels &destination, std::uint64_t timeout) const {
        if (!wait(timeout))
            throw std::runtime_error("Readback timed out");
        const auto &data = *_data;
        destination.bytes.resize(data.info.byteSize);
        auto mapped = data.buffer->read(data.info.byteSize);
        std::memcpy(destination.bytes.data(), mapped.data(), mapped.size());
        destination.width = data.info.region.width;
        destination.height = data.info.region.height;
        destination.rowStride = data.info.rowStride;
        destination.format = data.info.format;
    }
    std::size_t ImageReadback::capacity() const { return _data && _data->buffer ? _data->buffer->size() : 0; }
}
