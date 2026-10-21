#pragma once
#include "../buffer_and_image_accessor.hpp"
#include "../command.hpp"

namespace VSL_NAMESPACE::command {
    // RenderPass外で範囲を転送し、後続のGPUアクセスへ可視化する。送信・待機は行わない。
    struct CopyBuffer : __Command {
        CopyBuffer(BufferAccessor* source, BufferAccessor* destination, std::size_t bytes,
                   std::size_t sourceOffset = 0, std::size_t destinationOffset = 0, bool hostRead = false);

        void validate() const;
        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) override;

    private:
        std::shared_ptr<_impl::Buffer_impl> _source;
        std::shared_ptr<_impl::Buffer_impl> _destination;
        std::size_t _bytes;
        std::size_t _sourceOffset;
        std::size_t _destinationOffset;
        bool _hostRead;
    };
} // namespace VSL_NAMESPACE::command
