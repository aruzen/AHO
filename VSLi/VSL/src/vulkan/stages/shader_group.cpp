#include <VSL/lifetime/owner_validation.hpp>
#include <VSL/vulkan/stages/shader_group.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vulkan/pipeline_layout.hpp>
#include <VSL/operation/shader_ops.hpp>
#include <vector>
#include <VSL/lifetime/owner_access.hpp>
#include <VSL/lifetime/pipeline_layout_storage.hpp>
#include <VSL/lifetime/shader_access.hpp>

namespace {
    using VSL_NAMESPACE::lifetime::detail::ShaderAccess;
}

VSL_NAMESPACE::pipeline_layout::ShaderGroup::ShaderGroup(std::string name, std::initializer_list<ShaderAccessor> shaders) {
    _data = std::make_shared<impl::pipeline_layout::ShaderGroupImpl>();
    _data->name = std::move(name);
    for (auto& shader : shaders) {
        if (!ShaderAccess::Owned(shader) || !ShaderAccess::Owned(shader).use_count())
            throw std::invalid_argument("Shader group requires owning shader accessors");
        _data->shaders.push_back(ShaderAccess::Owned(shader));
    }
}

std::shared_ptr<VSL_NAMESPACE::impl::pipeline_layout::ShaderGroupImpl> VSL_NAMESPACE::pipeline_layout::ShaderGroup::checked_state() const {
    if (!_data)
        throw std::logic_error("Shader group is empty");
    return _data;
}

void VSL_NAMESPACE::pipeline_layout::ShaderGroup::add_shader(ShaderAccessor shader) {
    const auto data = checked_state();
    if (!ShaderAccess::Owned(shader) || !ShaderAccess::Owned(shader).use_count())
        throw std::invalid_argument("Shader group requires owning shader accessors");
    data->shaders.push_back(ShaderAccess::Owned(std::move(shader)));
}

void VSL_NAMESPACE::pipeline_layout::ShaderGroup::remove_shader(std::string name) {
    const auto data = checked_state();
    auto shaders = data->shaders;
    std::erase_if(shaders, [&name](const auto& shader) { return shader->name == name; });
    data->shaders.swap(shaders);
}

void VSL_NAMESPACE::pipeline_layout::ShaderGroup::remove_shader(ShaderAccessor shader) {
    const auto data = checked_state();
    auto shaders = data->shaders;
    std::erase_if(shaders, [&shader](const auto& candidate) { return candidate.get() == ShaderAccess::Owned(shader).get(); });
    data->shaders.swap(shaders);
}

void VSL_NAMESPACE::pipeline_layout::ShaderGroup::injection(VSL_NAMESPACE::PipelineLayoutEditAccessor pl) {
    const auto data = checked_state();
    auto& info = pl.edit_recipe();
    auto shaders = info.shaders;
    auto shader_sources = info.shader_sources;
    auto sources = info.sources;

    for (const auto& itr : data->shaders) {
        if (!itr || !itr.use_count() || itr->device != pl.get_device().data_.get())
            throw std::invalid_argument("Shader group requires modules on the pipeline device");
        (void)VSL_NAMESPACE::operation::shader_stage(*itr);
        lifetime::detail::require_live_owner(itr);
        shader_sources.push_back(lifetime::retain_recipe_owner(sources, itr));
        shaders.push_back(itr.get());
    }
    info.shaders.swap(shaders);
    info.sources.swap(sources);
    info.shader_sources.swap(shader_sources);
}
