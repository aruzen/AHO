#pragma once
#include <VSL/lifetime/resource_use.hpp>

namespace VSL_NAMESPACE::lifetime {
    // Cpp内の解決済み参照から共通状態と受け取ったCBを接続する。
    ResourceUse resource_use_for_buffer(const std::shared_ptr<impl::BufferImpl>& buffer, ResourceUse constraint);
    ResourceUse resource_use_for_image(const std::shared_ptr<impl::ImageImpl>& image);
    ResourceUse resource_use_for_sampler(const std::shared_ptr<impl::SamplerImpl>& sampler);
} // namespace VSL_NAMESPACE::lifetime
