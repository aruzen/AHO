#include <VSL/vulkan/stages/shader_group.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vulkan/pipeline_layout.hpp>
#include <vector>

VSL_NAMESPACE::pipeline_layout::ShaderGroup::ShaderGroup(std::string name, std::initializer_list<ShaderAccessor> shaders) {
    data_ = std::shared_ptr<VSL_NAMESPACE::impl::pipeline_layout::ShaderGroupImpl>(new VSL_NAMESPACE::impl::pipeline_layout::ShaderGroupImpl);
    data_->name = name;
    for (auto& shader : shaders) {
        data_->shaders.push_back(shader.data_);
    }
}

void VSL_NAMESPACE::pipeline_layout::ShaderGroup::add_shader(ShaderAccessor shader) {
    data_->shaders.push_back(shader.data_);
}

void VSL_NAMESPACE::pipeline_layout::ShaderGroup::remove_shader(std::string name) {
    std::erase_if(data_->shaders, [&name](const auto& shader) { return shader->name == name; });
}

void VSL_NAMESPACE::pipeline_layout::ShaderGroup::remove_shader(ShaderAccessor shader) {
    std::erase_if(data_->shaders, [&shader](const auto& candidate) { return candidate.get() == shader.data_.get(); });
}

void VSL_NAMESPACE::pipeline_layout::ShaderGroup::injection(VSL_NAMESPACE::PipelineLayoutEditAccessor pl) {
    auto& info = *pl.data_->info;

    for (auto& itr : data_->shaders) {
        info.shader_stages.push_back(itr->shader_stage_create_info);
        info.shaders.push_back(itr);
    }
}
