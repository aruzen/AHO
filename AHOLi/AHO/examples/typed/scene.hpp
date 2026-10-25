#pragma once
#include <AHO/drawer/draw_phase.hpp>
#include <VSL/utils/typed_batch.hpp>
#include <filesystem>

namespace typed_example {
    namespace t = vsl::typed;
    struct Vertex {
        float position[2];
        float color[3];
    };
    struct SceneUniform {
        float tint[4];
    };
    struct DrawTransform {
        // translation xy, scale, angle in radians
        float transform[4];
    };

    using VertexRecord = t::VertexRecord<Vertex, VSL_VERTEX_MEMBER(Vertex, position), VSL_VERTEX_MEMBER(Vertex, color)>;
    using Input = t::TypedVertexInput<VertexRecord::Binding<0>>;
    using UniformLayout = t::BlockRecord<SceneUniform, VSL_BLOCK_MEMBER(SceneUniform, tint)>;
    using TransformLayout = t::BlockRecord<DrawTransform, VSL_BLOCK_MEMBER(DrawTransform, transform)>;
    using Set = t::DescriptorSet<0, t::UniformBinding<0, UniformLayout, vsl::ShaderFlag::VERTEX>>;
    using Resources = t::ResourceSchema<Set>;
    using Pushes = t::PushSchema<t::PushRange<TransformLayout, vsl::ShaderFlag::VERTEX>>;
    using Pipeline = t::TypedGraphicsPipeline<Input, Resources, Pushes>;

    class Scene {
        aho::Window _window;
        vsl::LogicalDeviceAccessor _device;
        t::TypedVertexBuffer<Vertex> _vertices;
        t::TypedIndexBuffer<std::uint16_t> _indices;
        t::TypedUniformBuffer<UniformLayout> _uniform;
        t::TypedDescriptorSet<Set::Layout> _set;
        vsl::Shader<vsl::ShaderType::VERTEX> _vertex_shader;
        vsl::Shader<vsl::ShaderType::FRAGMENT> _fragment_shader;
        std::optional<Pipeline> _pipeline;
        vsl::data_format::Format _format = vsl::data_format::UNDEFINED;
        size_t _pipeline_builds = 0;
        std::shared_ptr<aho::window::WindowResizeHookPlugin> _resize_hook;
        void rebuild_pipeline();

    public:
        Scene(aho::Window window, const std::filesystem::path& shaders);
        ~Scene();
        Scene(const Scene&) = delete;
        Scene& operator=(const Scene&) = delete;
        size_t pipeline_builds() const { return _pipeline_builds; }
        void record(aho::DrawPhase& phase, float time);
    };
} // namespace typed_example
