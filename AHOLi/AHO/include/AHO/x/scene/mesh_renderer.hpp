#pragma once
#include <AHO/define.hpp>
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
        MeshRenderer(engine::GraphicalEngine& engine, std::span<const MeshVertex> vertices,
                     const std::filesystem::path& vertexShader, const std::filesystem::path& fragmentShader,
                     unsigned pushConstantBytes = 128);
        ~MeshRenderer();
        MeshRenderer(const MeshRenderer&) = delete;
        MeshRenderer& operator=(const MeshRenderer&) = delete;
        // Call before acquiring DrawPhase; recreates depth/framebuffers/pipeline after a resize.
        void prepare();
        std::array<unsigned, 2> extent() const;
        // Composition point for another AHO renderer recording into this same depth pass.
        VSL_NAMESPACE::RenderPassAccessor renderPass() const;
        VSL_NAMESPACE::SwapchainAccessor swapchain() const;
        // Starts a color+depth pass. DrawPhase ends and submits it, allowing Screenshot to attach.
        void record(DrawPhase& phase, std::span<const std::byte> pushConstants);
    };
} // namespace AHO_NAMESPACE::x::scene
