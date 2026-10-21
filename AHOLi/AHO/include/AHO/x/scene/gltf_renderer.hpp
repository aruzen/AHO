#pragma once
#include <AHO/x/resource/gltf.hpp>
#include <AHO/x/scene/mesh_renderer.hpp>

namespace AHO_NAMESPACE::x::scene {
    // Opaque triangle glTF with four-influence GPU skinning. Shares the terrain's depth pass.
    // One renderer owns one animated instance; target and engine must outlive it.
    class GltfRenderer {
        struct Data;
        std::unique_ptr<Data> data;

    public:
        GltfRenderer(engine::GraphicalEngine& engine, MeshRenderer& target, resource::GltfModel model,
                     const std::filesystem::path& vertexShader, const std::filesystem::path& fragmentShader);
        ~GltfRenderer();
        GltfRenderer(const GltfRenderer&) = delete;
        GltfRenderer& operator=(const GltfRenderer&) = delete;
        void prepare();
        // After target.record(); once per DrawPhase. eyeNight is eye.xyz + night intensity.
        // Shader bindings: sampler at 0, joint mat4 palette at 1 with a final metadata matrix.
        void record(DrawPhase& phase, const resource::GltfPose& pose, const resource::GltfMatrix& instance,
                    const resource::GltfMatrix& viewProjection, std::array<float, 4> eyeNight);
    };
} // namespace AHO_NAMESPACE::x::scene
