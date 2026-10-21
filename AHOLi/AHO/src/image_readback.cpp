#include <AHO/image_readback.hpp>
#include <VSL/vulkan/commands/image_readback.hpp>
#include <VSL/vulkan/phase.hpp>
#include <VSL/vulkan/buffer.hpp>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace AHO_NAMESPACE {
    namespace {
        struct ReadbackSlot {
            std::optional<VSL_NAMESPACE::CommandManager> transfer;
            using Buffer = VSL_NAMESPACE::Buffer<VSL_NAMESPACE::MemoryType::TRANSFER_DESTINATION, VSL_NAMESPACE::MemoryProperty::HOST_VISIBLE>;
            std::optional<Buffer> buffer;
            std::optional<VSL_NAMESPACE::BufferAccessor::LocalBufferHolder> mapping;
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
        std::size_t max_requests;
        std::vector<std::shared_ptr<ReadbackSlot>> slots;
        ImageReadbackTicket latest;
        std::uint64_t next_request_id = 1;

        Data(VSL_NAMESPACE::CommandManager producer, std::size_t max_requests);
        void reserve(ReadbackSlot& slot, std::size_t bytes);
        VSL_NAMESPACE::command::ImageCopyInfo describe(VSL_NAMESPACE::ImageAccessor image, VSL_NAMESPACE::ImageLayout layout,
                                                       VSL_NAMESPACE::ImageRegion region) const;
        std::shared_ptr<ReadbackSlot> acquire(std::size_t bytes);
        ImageReadbackTicket record(VSL_NAMESPACE::ComputePhase& phase, std::shared_ptr<ReadbackSlot> slot,
                                   VSL_NAMESPACE::ImageAccessor image, VSL_NAMESPACE::ImageLayout layout,
                                   const VSL_NAMESPACE::command::ImageCopyInfo& info, std::uint64_t frame_id);
    };

    const ImageReadbackInfo& ImageReadbackTicket::info() const {
        if (!_data)
            throw std::logic_error("Readback ticket is empty");
        return _data->info;
    }

    VSL_NAMESPACE::Completion::Status ImageReadbackTicket::status() const {
        return _data ? _data->completion.status() : VSL_NAMESPACE::Completion::Status::CANCELLED;
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
        if (!_data || current == VSL_NAMESPACE::Completion::Status::PENDING || current == VSL_NAMESPACE::Completion::Status::CANCELLED)
            throw std::logic_error("Readback was not submitted or was cancelled");
        if (!wait(timeout))
            throw std::runtime_error("Readback timed out");
        const auto& meta = _data->info;
        destination.bytes.resize(meta.byte_size);
        auto mapped = _data->slot->mapping->read(meta.byte_size);
        std::memcpy(destination.bytes.data(), mapped.data(), mapped.size());
        destination.width = meta.region.width;
        destination.height = meta.region.height;
        destination.row_stride = meta.row_stride;
        destination.format = meta.format;
    }

    ImageReadback::Data::Data(VSL_NAMESPACE::CommandManager producer, std::size_t max_requests)
        : producer(std::move(producer)), max_requests(max_requests) {
        (void)this->producer.get_device();
        if (!max_requests)
            throw std::invalid_argument("Readback requires at least one slot");
        slots.push_back(std::make_shared<ReadbackSlot>());
    }

    void ImageReadback::Data::reserve(ReadbackSlot& slot, std::size_t bytes) {
        if (bytes && (!slot.buffer || slot.buffer->requested_size() < bytes)) {
            ReadbackSlot::Buffer next(producer.get_device(), bytes);
            auto mapping = next.data();
            slot.buffer = std::move(next);
            slot.mapping.emplace(std::move(mapping));
        }
    }

    VSL_NAMESPACE::command::ImageCopyInfo ImageReadback::Data::describe(VSL_NAMESPACE::ImageAccessor image, VSL_NAMESPACE::ImageLayout layout,
                                                                        VSL_NAMESPACE::ImageRegion region) const {
        auto info = VSL_NAMESPACE::command::ImageReadback::Describe(image, layout, region);
        if (image.get_device() != producer.get_device())
            throw std::invalid_argument("Readback requires an image on the worker's device");
        return info;
    }

    std::shared_ptr<ReadbackSlot> ImageReadback::Data::acquire(std::size_t bytes) {
        std::shared_ptr<ReadbackSlot> candidate;
        for (auto& slot : slots) {
            if (!slot->request.expired())
                continue;
            auto status = slot->completion.status();
            if (status == VSL_NAMESPACE::Completion::Status::PENDING || status == VSL_NAMESPACE::Completion::Status::SUBMITTED)
                continue;
            if (slot->buffer && slot->buffer->requested_size() >= bytes)
                return slot;
            if (!candidate || (slot->buffer && (!candidate->buffer || slot->buffer->requested_size() > candidate->buffer->requested_size())))
                candidate = slot;
        }
        if (candidate) {
            reserve(*candidate, bytes);
            return candidate;
        }
        if (slots.size() >= max_requests)
            return {};
        auto slot = std::make_shared<ReadbackSlot>();
        reserve(*slot, bytes);
        slots.push_back(slot);
        return slot;
    }

    ImageReadbackTicket ImageReadback::Data::record(VSL_NAMESPACE::ComputePhase& phase, std::shared_ptr<ReadbackSlot> slot,
                                                    VSL_NAMESPACE::ImageAccessor image, VSL_NAMESPACE::ImageLayout layout,
                                                    const VSL_NAMESPACE::command::ImageCopyInfo& info, std::uint64_t frame_id) {
        if (!next_request_id)
            throw std::overflow_error("Readback request id exhausted");
        auto request = std::make_shared<ImageReadbackTicket::Data>();
        request->slot = slot;
        request->completion = phase.completion();
        request->info = {next_request_id++, frame_id, info.region, info.format, info.row_stride, info.byte_size};
        phase << VSL_NAMESPACE::command::ImageReadback(&*slot->buffer, image, layout, info.region);
        slot->completion = request->completion;
        slot->request = request;
        return ImageReadbackTicket(std::move(request));
    }

    ImageReadback::ImageReadback(VSL_NAMESPACE::CommandManager manager, std::size_t initial_capacity, std::size_t max_requests) {
        auto data = std::make_shared<Data>(std::move(manager), max_requests);
        data->reserve(*data->slots.front(), initial_capacity);
        _data = std::move(data);
    }

    ImageReadback::ImageReadback(VSL_NAMESPACE::CommandManager manager, VSL_NAMESPACE::ImageAccessor image,
                                 VSL_NAMESPACE::ImageLayout layout, VSL_NAMESPACE::ImageRegion region)
        : ImageReadback(std::move(manager)) {
        capture(std::move(image), layout, region);
    }

    std::optional<ImageReadbackTicket> ImageReadback::capture_ticket(VSL_NAMESPACE::ImageAccessor image,
                                                                    VSL_NAMESPACE::ImageLayout layout,
                                                                    VSL_NAMESPACE::ImageRegion region, std::uint64_t frame_id) {
        if (!_data)
            throw std::logic_error("Readback handle is empty");
        auto info = _data->describe(image, layout, region);
        auto slot = _data->acquire(info.byte_size);
        if (!slot)
            return std::nullopt;
        if (!slot->transfer)
            slot->transfer = _data->producer.independent();
        auto phase = slot->transfer->start_phase<VSL_NAMESPACE::ComputePhase>();
        auto ticket = _data->record(phase, slot, image, layout, info, frame_id);
        if (phase.submit() != VSL_NAMESPACE::ComputePhase::SubmitResult::SUCCESS)
            throw std::runtime_error("Readback submission failed");
        return ticket;
    }

    std::optional<ImageReadbackTicket> ImageReadback::record(VSL_NAMESPACE::ComputePhase& phase,
                                                             VSL_NAMESPACE::ImageAccessor image, VSL_NAMESPACE::ImageLayout layout,
                                                             VSL_NAMESPACE::ImageRegion region, std::uint64_t frame_id) {
        if (!_data)
            throw std::logic_error("Readback handle is empty");
        auto info = _data->describe(image, layout, region);
        if (phase.state != VSL_NAMESPACE::ComputePhase::State::WAIT_SUBMIT)
            throw std::logic_error("Readback requires a recording phase");
        if (phase.manager.get_device() != _data->producer.get_device())
            throw std::invalid_argument("Readback phase device mismatch");
        auto slot = _data->acquire(info.byte_size);
        if (!slot)
            return std::nullopt;
        return _data->record(phase, slot, image, layout, info, frame_id);
    }

    bool ImageReadback::capture(VSL_NAMESPACE::ImageAccessor image, VSL_NAMESPACE::ImageLayout layout, VSL_NAMESPACE::ImageRegion region) {
        if (!_data)
            throw std::logic_error("Readback handle is empty");
        (void)_data->describe(image, layout, region);
        if (_data->latest && !_data->latest.ready())
            return false;
        _data->latest = {};
        auto ticket = capture_ticket(image, layout, region);
        if (!ticket)
            return false;
        _data->latest = std::move(*ticket);
        return true;
    }

    void ImageReadback::discard_latest() {
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
                    total += slot->buffer->requested_size();
        return total;
    }
} // namespace AHO_NAMESPACE
