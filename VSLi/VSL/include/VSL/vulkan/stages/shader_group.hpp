#pragma once
#include "../../define.hpp"
#include "../pv.hpp"

#include <initializer_list>

#include "../shader.hpp"
#include "../pipeline_layout.hpp"

namespace VSL_NAMESPACE::pipeline_layout {
    struct ShaderGroup {
        std::shared_ptr<impl::pipeline_layout::ShaderGroupImpl> data_;

        ShaderGroup(std::string name, std::initializer_list<ShaderAccessor> shaders);

        void add_shader(ShaderAccessor shader);

        void remove_shader(std::string name);
        void remove_shader(ShaderAccessor shader);

        void injection(VSL_NAMESPACE::PipelineLayoutEditAccessor pl);
    };
} // namespace VSL_NAMESPACE::pipeline_layout
