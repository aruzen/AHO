#include <AHO/resource/buffer.hpp>
#include <VSL/vulkan/commands/copy_buffer.hpp>
#include <vector>
#include <cstring>

namespace AHO_NAMESPACE::resource {
    struct Buffer<BufferType::General>::Data {
        std::vector<std::byte> bytes;
    };
    Buffer<BufferType::General>::Buffer(std::size_t size) : data_(std::make_shared<Data>()) { data_->bytes.resize(size); }
    void* Buffer<BufferType::General>::data() { return data_ ? data_->bytes.data() : nullptr; }
    const void* Buffer<BufferType::General>::data() const { return data_ ? data_->bytes.data() : nullptr; }
    std::span<std::byte> Buffer<BufferType::General>::bytes() { return data_ ? std::span<std::byte>(data_->bytes) : std::span<std::byte>{}; }
    std::span<const std::byte> Buffer<BufferType::General>::bytes() const { return data_ ? std::span<const std::byte>(data_->bytes) : std::span<const std::byte>{}; }
    std::size_t Buffer<BufferType::General>::size() const { return data_ ? data_->bytes.size() : 0; }
    std::size_t Buffer<BufferType::General>::capacity() const { return data_ ? data_->bytes.capacity() : 0; }
    void Buffer<BufferType::General>::reserve(std::size_t capacity) {
        if (!data_)
            throw std::logic_error("Buffer is empty");
        data_->bytes.reserve(capacity);
    }
    void Buffer<BufferType::General>::resize(std::size_t size) {
        if (!data_)
            throw std::logic_error("Buffer is empty");
        data_->bytes.resize(size);
    }
    void Buffer<BufferType::General>::assign(std::span<const std::byte> bytes) {
        if (!data_)
            throw std::logic_error("Buffer is empty");
        if (bytes.size() > size())
            resize(bytes.size());
        if (!bytes.empty())
            std::memmove(data(), bytes.data(), bytes.size());
        resize(bytes.size());
    }
    void Buffer<BufferType::General>::write(std::span<const std::byte> bytes, std::size_t offset) {
        if (!data_)
            throw std::logic_error("Buffer is empty");
        if (offset > size() || bytes.size() > size() - offset)
            throw std::out_of_range("Buffer write range");
        if (!bytes.empty())
            std::memmove(static_cast<std::byte*>(data()) + offset, bytes.data(), bytes.size());
    }
    bool detail::BufferState::busy() const {
        const auto status = use.status();
        return status == VSL_NAMESPACE::Completion::Status::Pending || status == VSL_NAMESPACE::Completion::Status::Submitted;
    }
    void detail::BufferState::requireIdle() const {
        if (busy())
            throw std::logic_error("Buffer is still in use");
    }
    std::size_t BufferBinding::size() const { return state_ ? state_->size : 0; }
    VSL_NAMESPACE::LogicalDeviceAccessor BufferBinding::device() const {
        if (!state_)
            throw std::logic_error("Buffer binding is empty");
        return state_->device;
    }
    VSL_NAMESPACE::BufferAccessor& BufferBinding::native() const {
        if (!state_)
            throw std::logic_error("Buffer binding is empty");
        if (!state_->native)
            throw std::logic_error("Buffer has no GPU storage");
        return *state_->native;
    }
    std::shared_ptr<void> BufferBinding::owner() const {
        if (!state_)
            throw std::logic_error("Buffer binding is empty");
        return state_->resource;
    }
    bool BufferBinding::hostReadable() const {
        if (!state_)
            throw std::logic_error("Buffer binding is empty");
        return state_->hostReadable && state_->native && VSL_NAMESPACE::contain(state_->native->getMemoryProperties(), VSL_NAMESPACE::MemoryProperty::HostVisible);
    }
    void BufferBinding::checkAvailable(const VSL_NAMESPACE::Completion& completion) const {
        if (!state_)
            throw std::logic_error("Buffer binding is empty");
        if (!completion || completion.status() != VSL_NAMESPACE::Completion::Status::Pending)
            throw std::logic_error("Buffer transfer requires a recording phase");
        if (state_->busy() && state_->use != completion)
            throw std::logic_error("Buffer is in use by another phase");
    }
    void BufferBinding::markUsed(VSL_NAMESPACE::Completion completion) const {
        if (!state_)
            throw std::logic_error("Buffer binding is empty");
        state_->use = std::move(completion);
    }
    BufferUpload::BufferUpload(BufferBinding destination, Buffer<BufferType::General> source, std::size_t offset)
        : destination_(std::move(destination)), source_(std::move(source)), destinationOffset_(offset), sourceOffset_(0) {}
    BufferUpload::BufferUpload(BufferBinding destination, BufferBinding source, std::size_t destinationOffset, std::size_t sourceOffset, std::optional<std::size_t> bytes)
        : destination_(std::move(destination)), source_(std::move(source)), destinationOffset_(destinationOffset), sourceOffset_(sourceOffset), bytes_(bytes) {}
    void BufferUpload::invoke(VSL_NAMESPACE::CommandPool pool, VSL_NAMESPACE::CommandBuffer buffer, VSL_NAMESPACE::CommandManager manager) {
        auto completion = buffer.completion();
        destination_.checkAvailable(completion);
        if (destination_.device() != manager.getDevice())
            throw std::invalid_argument("Buffer upload device mismatch");
        auto record = [&](BufferBinding source, std::size_t sourceOffset, std::size_t bytes) {
            source.checkAvailable(completion);
            if (source.device() != destination_.device())
                throw std::invalid_argument("Buffer upload device mismatch");
            if (destinationOffset_ > destination_.size() || bytes > destination_.size() - destinationOffset_)
                throw std::out_of_range("Buffer upload destination range");
            if (!bytes)
                return;
            VSL_NAMESPACE::command::CopyBuffer command(&source.native(), &destination_.native(), bytes, sourceOffset, destinationOffset_, destination_.hostReadable());
            command.validate();
            buffer.retain(source.owner());
            buffer.retain(destination_.owner());
            command.invoke(pool, buffer, manager);
            source.markUsed(completion);
            destination_.markUsed(completion);
        };
        if (auto* source = std::get_if<BufferBinding>(&source_)) {
            if (sourceOffset_ > source->size())
                throw std::out_of_range("Buffer upload source offset");
            const auto bytes = bytes_.value_or(source->size() - sourceOffset_);
            if (bytes > source->size() - sourceOffset_)
                throw std::out_of_range("Buffer upload source range");
            record(*source, sourceOffset_, bytes);
        } else {
            const auto& cpuSource = std::get<Buffer<BufferType::General>>(source_);
            if (destinationOffset_ > destination_.size() || cpuSource.size() > destination_.size() - destinationOffset_)
                throw std::out_of_range("Buffer upload destination range");
            if (!cpuSource.size())
                return;
            if (!destination_.state_->staging) {
                Buffer<BufferType::GPUFriendly> staging(destination_.device());
                destination_.state_->staging = staging.binding().state_;
            }
            Buffer<BufferType::GPUFriendly> staging(destination_.state_->staging);
            staging.assign(cpuSource.bytes());
            record(staging.binding(), 0, cpuSource.size());
        }
    }
} // namespace AHO_NAMESPACE::resource
