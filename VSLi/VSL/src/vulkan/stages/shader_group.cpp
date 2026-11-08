#include <VSL/vulkan/stages/shader_group.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vulkan/pipeline_layout.hpp>
#include <VSL/operation/shader_ops.hpp>
#include <vector>
#include <VSL/lifetime/owner_access.hpp>
#include <VSL/lifetime/pipeline_layout_storage.hpp>

VSL_NAMESPACE::pipeline_layout::ShaderGroup::ShaderGroup(std::string name, std::initializer_list<ShaderAccessor> shaders) {
    data_ = std::shared_ptr<VSL_NAMESPACE::impl::pipeline_layout::ShaderGroupImpl>(new VSL_NAMESPACE::impl::pipeline_layout::ShaderGroupImpl);
    data_->name = name;
    for (auto& shader : shaders) {
        if (!shader.data_ || !shader.data_.use_count())
            throw std::invalid_argument("Shader group requires owning shader accessors");
        data_->shaders.push_back(shader.data_);
    }
}

void VSL_NAMESPACE::pipeline_layout::ShaderGroup::add_shader(ShaderAccessor shader) {
    if (!shader.data_ || !shader.data_.use_count())
        throw std::invalid_argument("Shader group requires owning shader accessors");
    data_->shaders.push_back(shader.data_);
}

void VSL_NAMESPACE::pipeline_layout::ShaderGroup::remove_shader(std::string name) {
    std::erase_if(data_->shaders, [&name](const auto& shader) { return shader->name == name; });
}

void VSL_NAMESPACE::pipeline_layout::ShaderGroup::remove_shader(ShaderAccessor shader) {
    std::erase_if(data_->shaders, [&shader](const auto& candidate) { return candidate.get() == shader.data_.get(); });
}

void VSL_NAMESPACE::pipeline_layout::ShaderGroup::injection(VSL_NAMESPACE::PipelineLayoutEditAccessor pl) {
    auto& info = pl.edit_recipe();
    auto shaders = info.shaders;
    auto shader_sources = info.shader_sources;
    auto sources = info.sources;

    for (auto& itr : data_->shaders) {
        if (!itr || !itr.use_count() || itr->device != pl.get_device().data_.get())
            throw std::invalid_argument("Shader group requires modules on the pipeline device");
        (void)VSL_NAMESPACE::operation::shader_stage(*itr);
        shader_sources.push_back(lifetime::retain_recipe_owner(sources, lifetime::detail::owned_ref(itr, itr->owner_node)));
        shaders.push_back(itr.get());
    }
    info.shaders.swap(shaders);
    info.sources.swap(sources);
    info.shader_sources.swap(shader_sources);
}
