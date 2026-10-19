#include <VSL/vulkan/stages/shader_group.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vulkan/pipeline_layout.hpp>
#include <vector>

VSL_NAMESPACE::pipeline_layout::ShaderGroup::ShaderGroup(std::string name, std::initializer_list<ShaderAccessor> shaders) {
    _data = std::shared_ptr<VSL_NAMESPACE::_impl::pipeline_layout::ShaderGroup_impl>(new VSL_NAMESPACE::_impl::pipeline_layout::ShaderGroup_impl);
    _data->name = name;
    for (auto& shader : shaders) {
        _data->shaders.push_back(shader._data);
    }
}

void VSL_NAMESPACE::pipeline_layout::ShaderGroup::addShader(ShaderAccessor shader) {
    _data->shaders.push_back(shader._data);
}

void VSL_NAMESPACE::pipeline_layout::ShaderGroup::removeShader(std::string name) {
    std::erase_if(_data->shaders, [&name](const auto& shader) { return shader->name == name; });
}

void VSL_NAMESPACE::pipeline_layout::ShaderGroup::removeShader(ShaderAccessor shader) {
    std::erase_if(_data->shaders, [&shader](const auto& candidate) { return candidate.get() == shader._data.get(); });
}

void VSL_NAMESPACE::pipeline_layout::ShaderGroup::injection(VSL_NAMESPACE::PipelineLayoutAccessor pl) {
    auto& info = *pl._data->info;

    for (auto& itr : _data->shaders)
        info.shaderStages.push_back(itr->shaderStageCreateInfo);
}
