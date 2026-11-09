#pragma once
#include "../define.hpp"

namespace VSL_NAMESPACE::lifetime {
    namespace detail {
        struct IdentityAccess;
        struct BufferIdentityTag;
        struct DescriptorIdentityTag;
        struct PipelineIdentityTag;
        struct CommandPoolIdentityTag;
        struct CommandBufferIdentityTag;
        struct CommandManagerIdentityTag;
    } // namespace detail

    // 物理実体の比較だけを行う非所有値。比較する間は元の所有者を保持する。
    // schemaのResourceIdやNative handleへは変換しない。
    template<typename Tag>
    class ResourceIdentity {
        const void* _value = nullptr;
        friend struct detail::IdentityAccess;
        explicit ResourceIdentity(const void* value) : _value(value) {}

    public:
        ResourceIdentity() = default;
        bool operator==(const ResourceIdentity&) const = default;
    };
} // namespace VSL_NAMESPACE::lifetime

namespace VSL_NAMESPACE {
    using BufferIdentity = lifetime::ResourceIdentity<lifetime::detail::BufferIdentityTag>;
    using DescriptorIdentity = lifetime::ResourceIdentity<lifetime::detail::DescriptorIdentityTag>;
    using PipelineIdentity = lifetime::ResourceIdentity<lifetime::detail::PipelineIdentityTag>;
    using CommandPoolIdentity = lifetime::ResourceIdentity<lifetime::detail::CommandPoolIdentityTag>;
    using CommandBufferIdentity = lifetime::ResourceIdentity<lifetime::detail::CommandBufferIdentityTag>;
    using CommandManagerIdentity = lifetime::ResourceIdentity<lifetime::detail::CommandManagerIdentityTag>;
} // namespace VSL_NAMESPACE
