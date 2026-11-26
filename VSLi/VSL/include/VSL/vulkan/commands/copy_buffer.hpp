#pragma once
#include "../buffer_and_image_accessor.hpp"
#include "../command.hpp"

namespace VSL_NAMESPACE::command {
    // RenderPass外で範囲を転送し、後続のGPUアクセスへ可視化する。送信・待機は行わない。
    struct CopyBuffer : Command {
        CopyBuffer(BufferAccessor* source, BufferAccessor* destination, std::size_t bytes,
                   std::size_t source_offset = 0, std::size_t destination_offset = 0, bool host_read = false);

        void validate() const;
        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) override;

    private:
        std::shared_ptr<impl::BufferImpl> _source;
        std::shared_ptr<impl::BufferImpl> _destination;
        std::size_t _bytes;
        std::size_t _source_offset;
        std::size_t _destination_offset;
        bool _host_read;
    };
} // namespace VSL_NAMESPACE::command
