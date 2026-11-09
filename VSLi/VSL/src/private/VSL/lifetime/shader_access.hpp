#pragma once
#include <VSL/vulkan/shader.hpp>
#include <VSL/vulkan/stages/shader_group.hpp>

namespace VSL_NAMESPACE::impl {
    struct PipelineRecipe;
}
namespace VSL_NAMESPACE::lifetime::detail {
    struct ShaderAccess {
        static const OwnedRef<impl::ShaderImpl>& Owned(const ShaderAccessor& shader) { return shader._owner; }
        static OwnedRef<impl::ShaderImpl>& Owned(ShaderAccessor& shader) { return shader._owner; }
        static OwnedRef<impl::ShaderImpl> Owned(ShaderAccessor&& shader) { return std::move(shader._owner); }
        static OwnedRef<impl::ShaderImpl> Owned(const ShaderAccessor&& shader) { return shader._owner; }
        static ShaderAccessor FromOwned(OwnedRef<impl::ShaderImpl> owner) { return ShaderAccessor(std::move(owner)); }
        static ShaderAccessor FromShared(std::shared_ptr<impl::ShaderImpl> owner);
        static ShaderAccessor FromRecipe(const impl::PipelineRecipe& recipe, size_t index);
    };
    struct ShaderGroupAccess {
        static const std::shared_ptr<impl::pipeline_layout::ShaderGroupImpl>& Data(const pipeline_layout::ShaderGroup& group) { return group._data; }
    };
} // namespace VSL_NAMESPACE::lifetime::detail
