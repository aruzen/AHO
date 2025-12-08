//
// Created by morimoto_hibiki on 2025/10/24.
//

#ifndef AHO_ALL_PIPELINE_H
#define AHO_ALL_PIPELINE_H

#include "Layouts.h"

namespace AHO_NAMESPACE {
    /*
     * TODO
     * VSL/typed_safe_pipelineを用いて型安全機構を作ることを今後の目標とする
     */

    struct PipelineData {
        vsl::graphic_resource::BindingLayout binding_layout;
        vsl::PipelineLayoutAccessor pipeline_layout;
        vsl::PipelineAccessor pipeline;
        vsl::pipeline_layout::ShaderGroup shader_group;
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
        template<vsl::helper::constant_string_holder String>
        ShaderReflectGraphicPipeline()
    };
     */
#endif
    // TODO: typed_safe_pipelineと合わせて, make<StandardPipelineTag, DefinedUniformBufferTag>()のような形でファクトリメソッドorファクトリクラスをくむべき
    enum class StandardPipelineTag {
        // FIXME:[issue] (Triangle + RGBA) * 4 = 112 Byteなので四角形まで拡張するか考えるべき
        PushConstantTriangle, // Triangle * 3 + RGBA * 3
        BindBufferIndexedTriangle,
    };

    enum class DefinedDescriptorBufferTag {
        ModelViewPerspectiveUniformBuffer,
        ViewPerspectiveUniformBuffer,
        Texture2D,
        ModelPushConstants,
        VertexColorInput,
    };
}


#endif //AHO_ALL_PIPELINE_H
