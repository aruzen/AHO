#pragma once
#include <AHO/x/scene/render_target.hpp>
#include <glm/glm.hpp>
#include <filesystem>
#include <memory>
#include <span>
#include <cstddef>
#include <array>
#include <VSL/vulkan/render_pass.hpp>
#include <VSL/vulkan/swapchain.hpp>

namespace AHO_NAMESPACE {
    struct DrawPhase;
    namespace engine {
        struct GraphicalEngine;
    }
} // namespace AHO_NAMESPACE
namespace AHO_NAMESPACE::x::scene {
    // Experimental descriptor-free vertex stream. Material is interpreted by the caller's shaders.
    struct MeshVertex {
        glm::vec3 position, normal, color;
        float material = 0;
    };
    class MeshRenderer {
        struct Data;
        std::unique_ptr<Data> data;

    public:
        // Target and engine must outlive this renderer.
        MeshRenderer(engine::GraphicalEngine& engine, RenderTarget& target, std::span<const MeshVertex> vertices,
                     const std::filesystem::path& vertexShader, const std::filesystem::path& fragmentShader,
                     unsigned pushConstantBytes = 128);
        ~MeshRenderer();
        MeshRenderer(const MeshRenderer&) = delete;
        MeshRenderer& operator=(const MeshRenderer&) = delete;
        // Records inside the pass started by target.begin(). Resize updates arrive through the plugin.
        void record(DrawPhase& phase, std::span<const std::byte> pushConstants);
    };
} // namespace AHO_NAMESPACE::x::scene
