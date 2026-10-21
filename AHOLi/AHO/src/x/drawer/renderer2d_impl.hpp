#pragma once
#include <AHO/x/drawer/renderer2d.hpp>
#include <AHO/x/command/draw2d.hpp>
#include <VSL/vulkan/buffer.hpp>

namespace AHO_NAMESPACE::x {
    namespace detail {
        using VertexBuffer = VSL_NAMESPACE::Buffer<VSL_NAMESPACE::MemoryType::VertexBuffer,
                                                   VSL_NAMESPACE::MemoryProperty::HostVisible | VSL_NAMESPACE::MemoryProperty::HostCoherent>;
        struct Upload {
            VertexBuffer buffer;
            std::size_t capacity;
            Upload(VSL_NAMESPACE::LogicalDeviceAccessor device, std::size_t bytes);
        };

        struct Slot {
            std::vector<std::unique_ptr<Upload>> uploads;
            std::vector<std::weak_ptr<VSL_NAMESPACE::_impl::Submission_impl>> submissions;
            bool available();
        };

        struct Pipeline {
            VSL_NAMESPACE::LogicalDeviceAccessor device;
            VSL_NAMESPACE::PipelineLayout layout;
            VSL_NAMESPACE::GraphicsPipeline pipeline;
            Pipeline(VSL_NAMESPACE::LogicalDeviceAccessor device, VSL_NAMESPACE::RenderPassAccessor pass,
                     const std::filesystem::path& directory, std::uint32_t subpass);
            static VSL_NAMESPACE::PipelineLayout MakeLayout(VSL_NAMESPACE::LogicalDeviceAccessor device, const std::filesystem::path& directory);
        };
    } // namespace detail

    struct Renderer2D::Data {
        std::shared_ptr<detail::Pipeline> pipeline;
        std::vector<std::shared_ptr<detail::Slot>> slots;
        std::size_t limit;
    };

    struct command::Draw2DCommand::Data {
        std::shared_ptr<detail::Pipeline> pipeline;
        std::shared_ptr<detail::Slot> slot;
        std::vector<DrawBatch> batches;
        std::uint32_t width, height;
        std::weak_ptr<VSL_NAMESPACE::_impl::Submission_impl> submission;
        std::weak_ptr<VSL_NAMESPACE::_impl::CommandManager_impl> manager;
    };
} // namespace AHO_NAMESPACE::x
