#pragma once
#include <AHO/resource/buffer.hpp>
#include <VSL/vulkan/command.hpp>

namespace AHO_NAMESPACE::command {
    // RenderPass外で記録する。送信・完了待ちは呼び出し側で行う。
    class BufferUpload : public VSL_NAMESPACE::command::__Command {
        resource::BufferBinding _destination;
        std::variant<resource::Buffer<resource::BufferType::General>, resource::BufferBinding> _source;
        std::size_t _destinationOffset;
        std::size_t _sourceOffset;
        std::optional<std::size_t> _bytes;

    public:
        BufferUpload(resource::BufferBinding destination, resource::Buffer<resource::BufferType::General> source, std::size_t destinationOffset);
        BufferUpload(resource::BufferBinding destination, resource::BufferBinding source, std::size_t destinationOffset, std::size_t sourceOffset,
                     std::optional<std::size_t> bytes);
        void invoke(VSL_NAMESPACE::CommandPool pool, VSL_NAMESPACE::CommandBuffer buffer, VSL_NAMESPACE::CommandManager manager) override;
    };
} // namespace AHO_NAMESPACE::command

namespace AHO_NAMESPACE::resource {
    template<typename Storage, typename Customization>
    AHO_NAMESPACE::command::BufferUpload detail::GPUBufferBase<Storage, Customization>::upload(const Buffer<BufferType::General>& source, std::size_t offset) const {
        return AHO_NAMESPACE::command::BufferUpload(binding(), source, offset);
    }

    template<typename Storage, typename Customization>
    template<AHO_NAMESPACE::resource::GPUBuffer Source>
    AHO_NAMESPACE::command::BufferUpload detail::GPUBufferBase<Storage, Customization>::upload(const Source& source, std::size_t destinationOffset,
                                                                                               std::size_t sourceOffset, std::optional<std::size_t> bytes) const {
        return AHO_NAMESPACE::command::BufferUpload(binding(), source.binding(), destinationOffset, sourceOffset, bytes);
    }
} // namespace AHO_NAMESPACE::resource
