#pragma once
#include "../define.hpp"
#include "buffer_and_image_accessor.hpp"
#include "command.hpp"

#include <compare>

#ifdef _MSC_VER
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(__APPLE_CC__)
#define VK_USE_PLATFORM_MACOS_MVK
#endif
#define GLFW_INCLUDE_VULKAN

#include <GLFW/glfw3.h>
#pragma warning(push)
#include <compare>
#define VULKAN_HPP_HAS_SPACESHIP_OPERATOR
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan.hpp>
#pragma warning(pop)

#include <optional>
#include <memory>
#include <map>
#include <any>
#include <set>

namespace VSL_NAMESPACE {
    namespace graphic_resource {
        struct ManagerInterface;
    }
    struct RenderPassDescription;
} // namespace VSL_NAMESPACE

namespace VSL_NAMESPACE::_impl {
    struct Vulkan_impl_accessor {
        vk::Instance instance;
        vk::DebugUtilsMessengerEXT debugMessenger;

        virtual ~Vulkan_impl_accessor() {};
    };

    template<bool V>
    struct Vulkan_impl : public Vulkan_impl_accessor {
        virtual ~Vulkan_impl() override;
    };

    struct PhysicalDevice_impl {
        std::shared_ptr<Vulkan_impl_accessor> vulkan;

        vk::PhysicalDevice device;
        std::optional<vk::PhysicalDeviceProperties> props;
        std::optional<vk::PhysicalDeviceFeatures> features;
        std::optional<vk::PhysicalDeviceMemoryProperties> memProps;

        void makeProps();
        void makeMemProps();
    };

    struct PhysicalDevices_impl {
        std::shared_ptr<Vulkan_impl_accessor> vulkan;

        std::vector<vk::PhysicalDevice> devices = {};
    };

    struct Surface_impl {
        std::shared_ptr<Vulkan_impl_accessor> vulkan;

        vk::SurfaceKHR surface;
        void* window;

        ~Surface_impl();
    };

    struct LogicalDevice_impl {
        std::shared_ptr<PhysicalDevice_impl> parentDevice;

        uint32_t graphicsFamily = 0, presentFamily = 0;
        vk::Device device;

        ~LogicalDevice_impl();
    };

    struct CommandPool_impl {
        std::shared_ptr<LogicalDevice_impl> device;

        vk::CommandPool commandPool;
        ~CommandPool_impl();
    };

    struct Submission_impl {
        std::shared_ptr<LogicalDevice_impl> device;
        std::shared_ptr<vk::UniqueFence> ownedFence;
        std::shared_ptr<Fence_impl> externalFence;
        vk::Fence fence;
        Completion::Status status = Completion::Status::Pending;
        std::vector<std::shared_ptr<void>> resources;
        ~Submission_impl();
    };

    struct CommandBuffer_impl {
        std::shared_ptr<CommandPool_impl> commandPool;

        std::uint32_t currentBufferIdx;

        std::vector<vk::CommandBuffer> commandBuffers;
        std::vector<std::shared_ptr<Submission_impl>> submissions;
        std::shared_ptr<Submission_impl> recordingSubmission;
        std::optional<std::uint32_t> acquiredImageIndex;
        std::weak_ptr<Swapchain_impl> acquiredSwapchain;
        bool recording = false;

        ~CommandBuffer_impl();
    };

    struct CommandManager_impl {
        std::shared_ptr<LogicalDevice_impl> device;

        std::shared_ptr<CommandPool_impl> commandPool;
        std::shared_ptr<CommandBuffer_impl> commandBuffer;

        vk::Queue graphicsQueue, presentQueue;
    };

    struct Swapchain_impl {
        std::shared_ptr<LogicalDevice_impl> device;
        std::shared_ptr<Surface_impl> surface;

        vk::SurfaceFormatKHR surfaceFormat;
        vk::PresentModeKHR presentMode;
        vk::Extent2D extent;
        uint32_t imageCount;
        vk::SurfaceTransformFlagBitsKHR preTransform;

        vk::SwapchainKHR swapChain;
        std::vector<vk::Image> swapChainImages;
        vk::Format swapChainImageFormat;
        vk::Extent2D swapChainExtent;
        std::vector<vk::UniqueSemaphore> presentationSemaphores;

        ~Swapchain_impl();
    };

    struct CreateInfo {
        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
        std::vector<std::shared_ptr<Shader_impl>> shaders;
        std::vector<std::shared_ptr<ResourceBindingLayout_impl>> bindingLayouts;
        vk::PipelineDynamicStateCreateInfo dynamicState;
        vk::PipelineLayoutCreateInfo pipelineLayout;
        vk::PipelineVertexInputStateCreateInfo vertexInput;
        vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
        std::vector<vk::Viewport> viewports;
        std::vector<vk::Rect2D> scissors;
        vk::PipelineRasterizationStateCreateInfo rasterization;
        vk::PipelineMultisampleStateCreateInfo multisample;
        vk::PipelineColorBlendStateCreateInfo colorBlend;

        std::optional<vk::PipelineDepthStencilStateCreateInfo> depthStencil;

        vk::PipelineViewportStateCreateInfo _viewport;

        std::vector<vk::DynamicState> enabledDynamicStates;
        std::map<std::string, std::any> pool;
    };

    struct PipelineLayout_impl {
        vk::PipelineLayout pipelineLayout;
        std::shared_ptr<CreateInfo> info;
        std::shared_ptr<LogicalDevice_impl> device;

        ~PipelineLayout_impl();
    };

    struct Shader_impl {
        std::string name;
        std::shared_ptr<LogicalDevice_impl> device;
        vk::ShaderModule shaderModule;
        vk::PipelineShaderStageCreateInfo shaderStageCreateInfo;

        ~Shader_impl();
    };

    struct RenderPass_impl {
        std::shared_ptr<LogicalDevice_impl> device;
        vk::RenderPass renderPass;
        std::shared_ptr<const RenderPassDescription> description;

        ~RenderPass_impl();
    };

    struct Pipeline_impl {
        std::shared_ptr<LogicalDevice_impl> device;
        std::shared_ptr<PipelineLayout_impl> layout;

        vk::Pipeline pipeline;
        vk::PipelineBindPoint bindPoint = vk::PipelineBindPoint::eGraphics;

        ~Pipeline_impl();
    };

    struct Image_impl;

    struct FrameBuffer_impl {
        std::shared_ptr<LogicalDevice_impl> device;
        std::shared_ptr<Swapchain_impl> swapchain;

        std::vector<vk::Framebuffer> swapChainFramebuffers;
        std::shared_ptr<RenderPass_impl> renderPass;
        std::vector<std::vector<std::shared_ptr<Image_impl>>> attachments;
        vk::Extent2D extent;
        std::uint32_t layers = 1;

        ~FrameBuffer_impl();
    };

    struct SynchroManager_impl;
    struct Semaphore_impl {
        std::weak_ptr<SynchroManager_impl> _manager;
        std::shared_ptr<LogicalDevice_impl> device;

        std::vector<vk::Semaphore> semaphores;
        ~Semaphore_impl();
    };

    struct Fence_impl {
        std::weak_ptr<SynchroManager_impl> _manager;
        std::shared_ptr<LogicalDevice_impl> device;

        std::vector<vk::Fence> fences;
        std::vector<std::weak_ptr<Submission_impl>> submissions;
        ~Fence_impl();
    };

    struct SynchroManager_impl {
        std::shared_ptr<LogicalDevice_impl> device;

        std::map<std::string, std::shared_ptr<Semaphore_impl>> semaphores;
        std::map<std::string, std::shared_ptr<Fence_impl>> fences;

        ~SynchroManager_impl();
    };

    struct Buffer_impl {
        std::shared_ptr<LogicalDevice_impl> device;

        vk::Buffer buffer;
        vk::DeviceMemory deviceMem;
        size_t requirementSize, allocatedSize = 0;

        ~Buffer_impl();
    };

    struct MappedBuffer_impl {
        std::shared_ptr<Buffer_impl> buffer;
        MemoryType usage = MemoryType::None;
        void* mapped = nullptr;
        bool coherent = false;
        vk::DeviceSize atomSize = 1;
        ~MappedBuffer_impl();
    };

    struct ResourceBindingLayout_impl {
        std::shared_ptr<LogicalDevice_impl> device;

        std::vector<vk::DescriptorSetLayoutBinding> layoutBindings;
        vk::DescriptorSetLayout layout;

        ~ResourceBindingLayout_impl();
    };

    struct GraphicResourceManagerIdentity {
        graphic_resource::ManagerInterface* manager = nullptr;
    };

    struct GraphicResourcePool_impl;
    struct GraphicResourceManager_impl {
        std::shared_ptr<LogicalDevice_impl> device;

        std::vector<std::shared_ptr<GraphicResourcePool_impl>> pools;

        ~GraphicResourceManager_impl();
    };

    struct GraphicResource_impl;
    struct GraphicResourcePool_impl {
        std::weak_ptr<GraphicResourceManagerIdentity> manager;
        std::shared_ptr<LogicalDevice_impl> device;

        vk::DescriptorPool descriptorPool;
        std::vector<std::weak_ptr<GraphicResource_impl>> resources;

        ~GraphicResourcePool_impl();
    };

    struct GraphicResource_impl {
        std::shared_ptr<_impl::GraphicResourcePool_impl> pool;
        std::shared_ptr<_impl::ResourceBindingLayout_impl> layout;

        vk::DescriptorSet descriptorSet;
        std::map<std::uint32_t, std::vector<std::shared_ptr<void>>> bindings;

        ~GraphicResource_impl();
    };

    struct Image_impl {
        std::shared_ptr<LogicalDevice_impl> device;
        vk::Image image;
        std::shared_ptr<Swapchain_impl> swapchain;
        ImageType usage = ImageType::None;
        SampleCount samples = SampleCount::One;
        vk::ImageView view;
        vk::DeviceMemory memory;
        std::uint32_t width, height, count;
        vk::Format format;

        ~Image_impl();
    };

    struct Sampler_impl {
        std::shared_ptr<LogicalDevice_impl> device;
        vk::Sampler sampler;

        ~Sampler_impl();
    };

    namespace pipeline_layout {
        struct ShaderGroup_impl {
            std::string name;
            std::vector<std::shared_ptr<Shader_impl>> shaders;
        };

    } // namespace pipeline_layout

    namespace helper {
        struct QueueFamilyIndices {
            std::optional<uint32_t> graphicsFamily;
            std::optional<uint32_t> presentFamily;

            bool isComplete();
        };

        std::uint32_t findMemoryType(std::shared_ptr<VSL_NAMESPACE::_impl::LogicalDevice_impl> device,
                                     std::uint32_t typeFilter,
                                     vk::MemoryPropertyFlags properties);
        QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device, vk::SurfaceKHR surface);
        void DestroyDebugUtilsMessengerEXT(vk::Instance& instance, vk::DebugUtilsMessengerEXT& debugMessenger, const VkAllocationCallbacks* pAllocator);
    } // namespace helper
} // namespace VSL_NAMESPACE::_impl
