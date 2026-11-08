#pragma once
#include <VSL/define.hpp>
#include <VSL/operation/pipeline_layout_create_info.hpp>
#include <VSL/operation/render_pass_description.hpp>
#include <VSL/vulkan/buffer_and_image_accessor.hpp>
#include <VSL/vulkan/command.hpp>
#include <VSL/lifetime/resource_use.hpp>

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
    class ResourceUseState;
    namespace lifetime {
        struct DescriptorOwnerContext;
        struct FrameBufferOwnerContext;
        struct CommandDomain;
        struct CommandRecordingContext;
        struct CommandBufferOwnerContext;
    } // namespace lifetime
    namespace graphic_resource {
        struct ManagerInterface;
    }
    struct RenderPassDescription;
} // namespace VSL_NAMESPACE

namespace VSL_NAMESPACE::impl {
    struct VulkanImplAccessor {
        vk::Instance instance;
        vk::DebugUtilsMessengerEXT debug_messenger;

        virtual ~VulkanImplAccessor() {};
    };

    template<bool v>
    struct VulkanImpl : public VulkanImplAccessor {
        virtual ~VulkanImpl() override;
    };

    struct PhysicalDeviceImpl {
        std::shared_ptr<VulkanImplAccessor> vulkan;

        vk::PhysicalDevice device;
        std::optional<vk::PhysicalDeviceProperties> props;
        std::optional<vk::PhysicalDeviceFeatures> features;
        std::optional<vk::PhysicalDeviceMemoryProperties> mem_props;

        void make_props();
        void make_mem_props();
    };

    struct PhysicalDevicesImpl {
        std::shared_ptr<VulkanImplAccessor> vulkan;

        std::vector<vk::PhysicalDevice> devices = {};
    };

    struct SurfaceImpl {
        std::shared_ptr<VulkanImplAccessor> vulkan;

        vk::SurfaceKHR surface;
        void* window;

        ~SurfaceImpl();
    };

    struct LogicalDeviceImpl : std::enable_shared_from_this<LogicalDeviceImpl> {
        std::shared_ptr<PhysicalDeviceImpl> parent_device;

        uint32_t graphics_family = 0, present_family = 0;
        vk::Device device;

        ~LogicalDeviceImpl();
    };

    struct CommandPoolImpl {
        CommandPoolImpl() = default;
        CommandPoolImpl(const CommandPoolImpl&) = delete;
        CommandPoolImpl& operator=(const CommandPoolImpl&) = delete;
        lifetime::DependencyNode* owner_node = nullptr;
        LogicalDeviceImpl* device = nullptr;
        vk::Device native_device;
        vk::CommandPool command_pool;
        ~CommandPoolImpl();
    };

    struct SubmissionImpl {
        Completion::Status status = Completion::Status::PENDING;
        Completion::Failure failure = Completion::Failure::NONE;
        std::int32_t error_code = 0;
        std::weak_ptr<SubmissionWork> work;
    };
    struct SubmissionWork {
        std::shared_ptr<SubmissionImpl> result = std::make_shared<SubmissionImpl>();
        std::shared_ptr<LogicalDeviceImpl> device;
        std::shared_ptr<vk::UniqueFence> owned_fence;
        std::shared_ptr<FenceImpl> external_fence;
        vk::Fence fence;
        std::vector<std::shared_ptr<void>> resources;
        std::vector<std::shared_ptr<void>> completion_resources;
        void (*release_resources)(SubmissionWork&) noexcept = nullptr;
        ~SubmissionWork();
    };

    struct CommandBufferImpl {
        CommandBufferImpl() = default;
        CommandBufferImpl(const CommandBufferImpl&) = delete;
        CommandBufferImpl& operator=(const CommandBufferImpl&) = delete;
        lifetime::DependencyNode* owner_node = nullptr;
        CommandPoolImpl* command_pool = nullptr;
        vk::Device native_device;
        vk::CommandPool native_pool;
        std::uint32_t current_buffer_idx = 0;

        std::vector<vk::CommandBuffer> command_buffers;
        lifetime::CommandBufferOwnerContext* owner_context = nullptr;
        lifetime::CommandRecordingContext* recording_context = nullptr;

        ~CommandBufferImpl();
    };

    struct CommandManagerImpl {
        std::shared_ptr<LogicalDeviceImpl> device;

        std::shared_ptr<CommandPoolImpl> command_pool;
        std::shared_ptr<CommandBufferImpl> command_buffer;

        vk::Queue graphics_queue, present_queue;
    };

    struct SwapchainImpl {
        std::shared_ptr<LogicalDeviceImpl> device;
        std::shared_ptr<SurfaceImpl> surface;

        vk::SurfaceFormatKHR surface_format;
        vk::PresentModeKHR present_mode;
        vk::Extent2D extent;
        uint32_t image_count;
        vk::SurfaceTransformFlagBitsKHR pre_transform;

        vk::SwapchainKHR swap_chain;
        std::vector<vk::Image> swap_chain_images;
        vk::Format swap_chain_image_format;
        vk::Extent2D swap_chain_extent;
        std::vector<vk::UniqueSemaphore> presentation_semaphores;

        ~SwapchainImpl();
    };

    struct GraphicsPipelineStateData {
        std::vector<vk::VertexInputBindingDescription> vertex_bindings;
        std::vector<vk::VertexInputAttributeDescription> vertex_attributes;
        std::vector<vk::PipelineColorBlendAttachmentState> blend_attachments;
        vk::PipelineInputAssemblyStateCreateInfo input_assembly;
        std::vector<vk::Viewport> viewports;
        std::vector<vk::Rect2D> scissors;
        vk::PipelineRasterizationStateCreateInfo rasterization;
        vk::PipelineMultisampleStateCreateInfo multisample;
        vk::PipelineColorBlendStateCreateInfo color_blend;

        std::optional<vk::PipelineDepthStencilStateCreateInfo> depth_stencil;

        std::vector<vk::DynamicState> enabled_dynamic_states;
    };

    struct PipelineRecipe {
        // recipeはruntime rootの外に置く。同じsource ownerはここで一度だけ保持する。
        std::vector<lifetime::OwnerRef> sources;
        std::vector<size_t> shader_sources;
        std::vector<ShaderImpl*> shaders;
        std::vector<ResourceBindingLayoutImpl*> binding_layouts;
        std::vector<PushConstantRange> push_ranges;
        GraphicsPipelineStateData graphics_state;
    };

    struct PipelineLayoutImpl {
        lifetime::DependencyNode* owner_node = nullptr;
        LogicalDeviceImpl* device = nullptr;
        vk::Device native_device;
        vk::PipelineLayout pipeline_layout;
        // 記録とruntime/recipeの照合に必要な値だけを保持する。
        std::vector<vk::PushConstantRange> push_ranges;
        std::vector<std::vector<vk::DescriptorSetLayoutBinding>> binding_signatures;

        ~PipelineLayoutImpl();
    };

    struct ShaderImpl {
        lifetime::DependencyNode* owner_node = nullptr;
        std::string name;
        std::vector<std::uint32_t> code;
        LogicalDeviceImpl* device = nullptr;
        vk::Device native_device;
        vk::ShaderModule shader_module;
        vk::ShaderStageFlagBits stage = vk::ShaderStageFlagBits::eVertex;

        ~ShaderImpl();
    };

    struct RenderPassImpl {
        RenderPassImpl() = default;
        RenderPassImpl(const RenderPassImpl&) = delete;
        RenderPassImpl(RenderPassImpl&&) = delete;
        RenderPassImpl& operator=(const RenderPassImpl&) = delete;
        RenderPassImpl& operator=(RenderPassImpl&&) = delete;
        lifetime::DependencyNode* owner_node = nullptr;
        LogicalDeviceImpl* device = nullptr;
        vk::Device native_device;
        vk::RenderPass render_pass;
        std::optional<const RenderPassDescription> description;

        ~RenderPassImpl();
    };

    struct PipelineImpl {
        PipelineImpl() = default;
        PipelineImpl(const PipelineImpl&) = delete;
        PipelineImpl(PipelineImpl&&) = delete;
        PipelineImpl& operator=(const PipelineImpl&) = delete;
        PipelineImpl& operator=(PipelineImpl&&) = delete;
        lifetime::DependencyNode* owner_node = nullptr;
        LogicalDeviceImpl* device = nullptr;
        vk::Device native_device;
        PipelineLayoutImpl* layout = nullptr;
        vk::Pipeline pipeline;
        vk::PipelineBindPoint bind_point = vk::PipelineBindPoint::eGraphics;

        ~PipelineImpl();
    };

    struct ImageImpl;

    struct FrameBufferImpl {
        FrameBufferImpl() = default;
        FrameBufferImpl(const FrameBufferImpl&) = delete;
        FrameBufferImpl(FrameBufferImpl&&) = delete;
        FrameBufferImpl& operator=(const FrameBufferImpl&) = delete;
        FrameBufferImpl& operator=(FrameBufferImpl&&) = delete;
        lifetime::DependencyNode* owner_node = nullptr;
        LogicalDeviceImpl* device = nullptr;
        vk::Device native_device;
        lifetime::FrameBufferOwnerContext* owner_context = nullptr;
        std::vector<vk::Framebuffer> swap_chain_framebuffers;
        RenderPassImpl* render_pass = nullptr;
        std::vector<std::vector<ImageImpl*>> attachments;
        vk::Extent2D extent;
        std::uint32_t layers = 1;

        ~FrameBufferImpl();
    };

    struct SynchroManagerImpl;
    struct SemaphoreImpl {
        std::weak_ptr<SynchroManagerImpl> manager;
        std::shared_ptr<LogicalDeviceImpl> device;

        std::vector<vk::Semaphore> semaphores;
        ~SemaphoreImpl();
    };

    struct FenceImpl {
        std::weak_ptr<SynchroManagerImpl> manager;
        std::shared_ptr<LogicalDeviceImpl> device;

        std::vector<vk::Fence> fences;
        std::vector<std::weak_ptr<SubmissionImpl>> submissions;
        ~FenceImpl();
    };

    struct SynchroManagerImpl {
        std::shared_ptr<LogicalDeviceImpl> device;

        std::map<std::string, std::shared_ptr<SemaphoreImpl>> semaphores;
        std::map<std::string, std::shared_ptr<FenceImpl>> fences;

        ~SynchroManagerImpl();
    };

    struct BufferImpl {
        lifetime::DependencyNode* owner_node = nullptr;
        LogicalDeviceImpl* device = nullptr;
        vk::Device native_device;
        ResourceUseState* use = nullptr;

        vk::Buffer buffer;
        vk::DeviceMemory device_mem;
        size_t requirement_size = 0, allocated_size = 0;
        MemoryType usage = MemoryType::NONE;
        MemoryProperty properties = MemoryProperty::NONE;
        void* mapped = nullptr;
        vk::DeviceSize atom_size = 1;
    };

    struct ResourceBindingLayoutImpl {
        lifetime::DependencyNode* owner_node = nullptr;
        LogicalDeviceImpl* device = nullptr;
        vk::Device native_device;

        std::vector<vk::DescriptorSetLayoutBinding> layout_bindings;
        vk::DescriptorSetLayout layout;

        ~ResourceBindingLayoutImpl();
    };

    struct GraphicResourceManagerIdentity {
        graphic_resource::ManagerInterface* manager = nullptr;
    };

    struct GraphicResourcePoolImpl;
    struct GraphicResourceManagerImpl {
        std::shared_ptr<LogicalDeviceImpl> device;

        std::vector<std::shared_ptr<GraphicResourcePoolImpl>> pools;

        ~GraphicResourceManagerImpl();
    };

    struct GraphicResourceImpl;
    struct GraphicResourcePoolImpl {
        lifetime::DependencyNode* owner_node = nullptr;
        struct Capacity {
            std::uint32_t limit = 0, allocated = 0;
        };
        std::weak_ptr<GraphicResourceManagerIdentity> manager;
        LogicalDeviceImpl* device = nullptr;
        vk::Device native_device;
        // getterの所有境界でだけ解決する。内部Setはpoolを直接借用する。
        std::weak_ptr<void> owner;

        vk::DescriptorPool descriptor_pool;
        std::uint32_t max_sets = 0, allocated_sets = 0;
        std::map<vk::DescriptorType, Capacity> capacities;
        bool retiring = false;
        std::vector<std::weak_ptr<GraphicResourceImpl>> resources;

        ~GraphicResourcePoolImpl();
    };

    struct GraphicResourceImpl {
        struct BindingConstraint {
            std::vector<lifetime::OwnerRef> owners;
            std::vector<ResourceUseState*> states;
        };
        struct BindingElement {
            std::vector<lifetime::OwnerRef> owners;
            // ownersに属する物理実体を借用する。論理制約も同じ要素で管理する。
            std::vector<ResourceUseState*> states;
            BindingConstraint constraint;
            bool initialized = false;
        };
        impl::GraphicResourcePoolImpl* pool = nullptr;
        lifetime::DependencyNode* owner_node = nullptr;
        impl::ResourceBindingLayoutImpl* layout = nullptr;
        // private metadataを省略したviewでも、解放時の割当数は元layoutで戻す。
        impl::ResourceBindingLayoutImpl* allocation_layout = nullptr;
        vk::Device native_device;
        lifetime::DescriptorOwnerContext* owner_context = nullptr;

        vk::DescriptorSet descriptor_set;
        ResourceUseState use;
        bool recording = false;
        std::map<std::uint32_t, std::vector<BindingElement>> bindings;

        ~GraphicResourceImpl();
    };

    struct ImageImpl {
        lifetime::DependencyNode* owner_node = nullptr;
        LogicalDeviceImpl* device = nullptr;
        vk::Device native_device;
        ResourceUseState* use = nullptr;
        vk::Image image;
        bool owns_image = false;
        ImageType usage = ImageType::NONE;
        SampleCount samples = SampleCount::ONE;
        vk::ImageView view;
        vk::DeviceMemory memory;
        std::uint32_t width = 0, height = 0, count = 0;
        vk::Format format = vk::Format::eUndefined;
    };

    struct SamplerImpl {
        lifetime::DependencyNode* owner_node = nullptr;
        LogicalDeviceImpl* device = nullptr;
        vk::Device native_device;
        ResourceUseState* use = nullptr;
        vk::Sampler sampler;
    };

    namespace pipeline_layout {
        struct ShaderGroupImpl {
            std::string name;
            std::vector<std::shared_ptr<ShaderImpl>> shaders;
        };

    } // namespace pipeline_layout

    namespace helper {
        struct QueueFamilyIndices {
            std::optional<uint32_t> graphics_family;
            std::optional<uint32_t> present_family;

            bool is_complete();
        };

        QueueFamilyIndices find_queue_families(vk::PhysicalDevice device, vk::SurfaceKHR surface);
        void destroy_debug_utils_messenger_ext(vk::Instance& instance, vk::DebugUtilsMessengerEXT& debug_messenger, const VkAllocationCallbacks* p_allocator);
    } // namespace helper
} // namespace VSL_NAMESPACE::impl
