#pragma once
#include "../buffer_and_image_accessor.hpp"
#include "../command.hpp"
#include "../buffer_view.hpp"

namespace VSL_NAMESPACE::command {
    // RenderPass外で範囲を転送し、後続のGPUアクセスへ可視化する。送信・待機は行わない。
    struct CopyBuffer : Command {
        CopyBuffer(BufferAccessor* source, BufferAccessor* destination, std::size_t bytes,
                   std::size_t source_offset = 0, std::size_t destination_offset = 0, bool host_read = false);
        CopyBuffer(BufferView source, BufferView destination, std::size_t bytes,
                   std::size_t source_offset = 0, std::size_t destination_offset = 0, bool host_read = false);

        void validate() const;
        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) override;

    private:
        impl::BufferImpl* _source;
        impl::BufferImpl* _destination;
        lifetime::OwnerRef _source_owner, _destination_owner;
        bool _release_on_completion = false;
        std::size_t _bytes;
        std::size_t _source_offset;
        std::size_t _destination_offset;
        bool _host_read;
    };
} // namespace VSL_NAMESPACE::command
