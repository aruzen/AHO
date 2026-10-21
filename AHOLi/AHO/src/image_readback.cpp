#include <AHO/image_readback.hpp>
#include <VSL/vulkan/commands/image_readback.hpp>
#include <VSL/vulkan/phase.hpp>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace AHO_NAMESPACE {
    namespace {
        struct ReadbackSlot {
            std::optional<VSL_NAMESPACE::CommandManager> transfer;
            std::optional<VSL_NAMESPACE::MappedBuffer> buffer;
            VSL_NAMESPACE::Completion completion;
            std::weak_ptr<void> request;
        };
    } // namespace

    struct ImageReadbackTicket::Data {
        std::shared_ptr<ReadbackSlot> slot;
        VSL_NAMESPACE::Completion completion;
        ImageReadbackInfo info;
    };

    struct ImageReadback::Data {
        VSL_NAMESPACE::CommandManager producer;
        std::size_t maxRequests;
        std::vector<std::shared_ptr<ReadbackSlot>> slots;
        ImageReadbackTicket latest;
        std::uint64_t nextRequestId = 1;

        Data(VSL_NAMESPACE::CommandManager producer, std::size_t maxRequests);
        void reserve(ReadbackSlot& slot, std::size_t bytes);
        VSL_NAMESPACE::command::ImageCopyInfo describe(VSL_NAMESPACE::ImageAccessor image, VSL_NAMESPACE::ImageLayout layout,
                                                       VSL_NAMESPACE::ImageRegion region) const;
        std::shared_ptr<ReadbackSlot> acquire(std::size_t bytes);
        ImageReadbackTicket record(VSL_NAMESPACE::ComputePhase& phase, std::shared_ptr<ReadbackSlot> slot,
                                   VSL_NAMESPACE::ImageAccessor image, VSL_NAMESPACE::ImageLayout layout,
                                   const VSL_NAMESPACE::command::ImageCopyInfo& info, std::uint64_t frameId);
    };

    const ImageReadbackInfo& ImageReadbackTicket::info() const {
        if (!_data)
            throw std::logic_error("Readback ticket is empty");
        return _data->info;
    }

    VSL_NAMESPACE::Completion::Status ImageReadbackTicket::status() const {
        return _data ? _data->completion.status() : VSL_NAMESPACE::Completion::Status::Cancelled;
    }

    bool ImageReadbackTicket::ready() const {
        return _data && _data->completion.ready();
    }

    bool ImageReadbackTicket::wait(std::uint64_t timeout) const {
        if (!_data)
            throw std::logic_error("Readback ticket is empty");
        return _data->completion.wait(timeout);
    }

    ImagePixels ImageReadbackTicket::read(std::uint64_t timeout) const {
        ImagePixels result;
        read(result, timeout);
        return result;
    }

    void ImageReadbackTicket::read(ImagePixels& destination, std::uint64_t timeout) const {
        const auto current = status();
        if (!_data || current == VSL_NAMESPACE::Completion::Status::Pending || current == VSL_NAMESPACE::Completion::Status::Cancelled)
            throw std::logic_error("Readback was not submitted or was cancelled");
        if (!wait(timeout))
            throw std::runtime_error("Readback timed out");
        const auto& meta = _data->info;
        destination.bytes.resize(meta.byteSize);
        auto mapped = _data->slot->buffer->read(meta.byteSize);
        std::memcpy(destination.bytes.data(), mapped.data(), mapped.size());
        destination.width = meta.region.width;
        destination.height = meta.region.height;
        destination.rowStride = meta.rowStride;
        destination.format = meta.format;
    }

    ImageReadback::Data::Data(VSL_NAMESPACE::CommandManager producer, std::size_t maxRequests)
        : producer(std::move(producer)), maxRequests(maxRequests) {
        (void)this->producer.getDevice();
        if (!maxRequests)
            throw std::invalid_argument("Readback requires at least one slot");
        slots.push_back(std::make_shared<ReadbackSlot>());
    }

    void ImageReadback::Data::reserve(ReadbackSlot& slot, std::size_t bytes) {
        if (bytes && (!slot.buffer || slot.buffer->size() < bytes)) {
            auto next = VSL_NAMESPACE::MappedBuffer(producer.getDevice(), bytes);
            slot.buffer = std::move(next);
        }
    }

    VSL_NAMESPACE::command::ImageCopyInfo ImageReadback::Data::describe(VSL_NAMESPACE::ImageAccessor image, VSL_NAMESPACE::ImageLayout layout,
                                                                        VSL_NAMESPACE::ImageRegion region) const {
        auto info = VSL_NAMESPACE::command::ImageReadback::describe(image, layout, region);
        if (image.getDevice() != producer.getDevice())
            throw std::invalid_argument("Readback requires an image on the worker's device");
        return info;
    }

    std::shared_ptr<ReadbackSlot> ImageReadback::Data::acquire(std::size_t bytes) {
        std::shared_ptr<ReadbackSlot> candidate;
        for (auto& slot : slots) {
            if (!slot->request.expired())
                continue;
            auto status = slot->completion.status();
            if (status == VSL_NAMESPACE::Completion::Status::Pending || status == VSL_NAMESPACE::Completion::Status::Submitted)
                continue;
            if (slot->buffer && slot->buffer->size() >= bytes)
                return slot;
            if (!candidate || (slot->buffer && (!candidate->buffer || slot->buffer->size() > candidate->buffer->size())))
                candidate = slot;
        }
        if (candidate) {
            reserve(*candidate, bytes);
            return candidate;
        }
        if (slots.size() >= maxRequests)
            return {};
        auto slot = std::make_shared<ReadbackSlot>();
        reserve(*slot, bytes);
        slots.push_back(slot);
        return slot;
    }

    ImageReadbackTicket ImageReadback::Data::record(VSL_NAMESPACE::ComputePhase& phase, std::shared_ptr<ReadbackSlot> slot,
                                                    VSL_NAMESPACE::ImageAccessor image, VSL_NAMESPACE::ImageLayout layout,
                                                    const VSL_NAMESPACE::command::ImageCopyInfo& info, std::uint64_t frameId) {
        if (!nextRequestId)
            throw std::overflow_error("Readback request id exhausted");
        auto request = std::make_shared<ImageReadbackTicket::Data>();
        request->slot = slot;
        request->completion = phase.completion();
        request->info = {nextRequestId++, frameId, info.region, info.format, info.rowStride, info.byteSize};
        phase << VSL_NAMESPACE::command::ImageReadback(*slot->buffer, image, layout, info.region);
        slot->completion = request->completion;
        slot->request = request;
        return ImageReadbackTicket(std::move(request));
    }

    ImageReadback::ImageReadback(VSL_NAMESPACE::CommandManager manager, std::size_t initialCapacity, std::size_t maxRequests) {
        auto data = std::make_shared<Data>(std::move(manager), maxRequests);
        data->reserve(*data->slots.front(), initialCapacity);
        _data = std::move(data);
    }

    ImageReadback::ImageReadback(VSL_NAMESPACE::CommandManager manager, VSL_NAMESPACE::ImageAccessor image,
                                 VSL_NAMESPACE::ImageLayout layout, VSL_NAMESPACE::ImageRegion region)
        : ImageReadback(std::move(manager)) {
        capture(std::move(image), layout, region);
    }

    std::optional<ImageReadbackTicket> ImageReadback::captureTicket(VSL_NAMESPACE::ImageAccessor image,
                                                                    VSL_NAMESPACE::ImageLayout layout,
                                                                    VSL_NAMESPACE::ImageRegion region, std::uint64_t frameId) {
        if (!_data)
            throw std::logic_error("Readback handle is empty");
        auto info = _data->describe(image, layout, region);
        auto slot = _data->acquire(info.byteSize);
        if (!slot)
            return std::nullopt;
        if (!slot->transfer)
            slot->transfer = _data->producer.independent();
        auto phase = slot->transfer->startPhase<VSL_NAMESPACE::ComputePhase>();
        auto ticket = _data->record(phase, slot, image, layout, info, frameId);
        if (phase.submit() != VSL_NAMESPACE::ComputePhase::SubmitResult::Success)
            throw std::runtime_error("Readback submission failed");
        return ticket;
    }

    std::optional<ImageReadbackTicket> ImageReadback::record(VSL_NAMESPACE::ComputePhase& phase,
                                                             VSL_NAMESPACE::ImageAccessor image, VSL_NAMESPACE::ImageLayout layout,
                                                             VSL_NAMESPACE::ImageRegion region, std::uint64_t frameId) {
        if (!_data)
            throw std::logic_error("Readback handle is empty");
        auto info = _data->describe(image, layout, region);
        if (phase.state != VSL_NAMESPACE::ComputePhase::State::WaitSubmit)
            throw std::logic_error("Readback requires a recording phase");
        if (phase.manager.getDevice() != _data->producer.getDevice())
            throw std::invalid_argument("Readback phase device mismatch");
        auto slot = _data->acquire(info.byteSize);
        if (!slot)
            return std::nullopt;
        return _data->record(phase, slot, image, layout, info, frameId);
    }

    bool ImageReadback::capture(VSL_NAMESPACE::ImageAccessor image, VSL_NAMESPACE::ImageLayout layout, VSL_NAMESPACE::ImageRegion region) {
        if (!_data)
            throw std::logic_error("Readback handle is empty");
        (void)_data->describe(image, layout, region);
        if (_data->latest && !_data->latest.ready())
            return false;
        _data->latest = {};
        auto ticket = captureTicket(image, layout, region);
        if (!ticket)
            return false;
        _data->latest = std::move(*ticket);
        return true;
    }

    void ImageReadback::discardLatest() {
        if (_data)
            _data->latest = {};
    }

    bool ImageReadback::ready() const {
        return _data && _data->latest.ready();
    }

    bool ImageReadback::wait(std::uint64_t timeout) const {
        if (!_data || !_data->latest)
            throw std::logic_error("Readback has no submitted capture");
        return _data->latest.wait(timeout);
    }

    ImagePixels ImageReadback::read(std::uint64_t timeout) const {
        ImagePixels result;
        read(result, timeout);
        return result;
    }

    void ImageReadback::read(ImagePixels& destination, std::uint64_t timeout) const {
        if (!_data || !_data->latest)
            throw std::logic_error("Readback has no submitted capture");
        _data->latest.read(destination, timeout);
    }

    std::size_t ImageReadback::capacity() const {
        std::size_t total = 0;
        if (_data)
            for (const auto& slot : _data->slots)
                if (slot->buffer)
                    total += slot->buffer->size();
        return total;
    }
} // namespace AHO_NAMESPACE
