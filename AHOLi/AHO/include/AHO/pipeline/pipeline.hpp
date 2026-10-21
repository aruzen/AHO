//
// Created by morimoto_hibiki on 2025/10/24.
//

#ifndef AHO_ALL_PIPELINE_H
#define AHO_ALL_PIPELINE_H

#include "layouts.hpp"

namespace AHO_NAMESPACE {
    /*
     * TODO
     * VSL/typed_safe_pipelineを用いて型安全機構を作ることを今後の目標とする
     */

    struct PipelineData {
        VSL_NAMESPACE::graphic_resource::BindingLayout binding_layout;
        VSL_NAMESPACE::PipelineLayoutAccessor pipeline_layout;
        VSL_NAMESPACE::PipelineAccessor pipeline;
        VSL_NAMESPACE::pipeline_layout::ShaderGroup shader_group;
    };

    // FIXME: Implement me
    /*
     * vk::PipelineがComputePipelineであることを保証してください
     */
    struct ComputePipeline : public PipelineData {};

    /*
     * vk::PipelineがGraphicsPipelineであることを保証してください
     */
    struct GraphicPipeline : public PipelineData {};

#ifdef VSL_SPIRV_REFLECTOR_UTIL
    /*
    struct ShaderReflectGraphicPipeline : public GraphicPipeline {
        template<vsl::helper::ConstantStringHolder string>
        ShaderReflectGraphicPipeline()
    };
     */
#endif
    // TODO: typed_safe_pipelineと合わせて, make<StandardPipelineTag, DefinedUniformBufferTag>()のような形でファクトリメソッドorファクトリクラスをくむべき
    enum class StandardPipelineTag {
        // FIXME:[issue] (Triangle + RGBA) * 4 = 112 Byteなので四角形まで拡張するか考えるべき
        PUSH_CONSTANT_TRIANGLE, // Triangle * 3 + RGBA * 3
        BIND_BUFFER_INDEXED_TRIANGLE,
    };

    enum class DefinedDescriptorBufferTag {
        MODEL_VIEW_PERSPECTIVE_UNIFORM_BUFFER,
        VIEW_PERSPECTIVE_UNIFORM_BUFFER,
        TEXTURE2_D,
        MODEL_PUSH_CONSTANTS,
        VERTEX_COLOR_INPUT,
    };
} // namespace AHO_NAMESPACE

#endif //AHO_ALL_PIPELINE_H
