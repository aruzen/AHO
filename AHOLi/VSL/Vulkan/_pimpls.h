#pragma once
#include "../define.h"

#include <compare>

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#pragma warning( push )
#include <compare>
// #define VULKAN_HPP_HAS_SPACESHIP_OPERATOR
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan.hpp>
#pragma warning( pop )

#include <optional>
#include <memory>
#include <map>
#include <any>

namespace VSL_NAMESPACE::_impl {
	struct Vulkan_impl_accesor {
		vk::Instance instance;
		vk::DebugUtilsMessengerEXT debugMessenger;

		virtual ~Vulkan_impl_accesor() {};
	};

	template<bool V>
	struct Vulkan_impl : public Vulkan_impl_accesor {
		virtual ~Vulkan_impl() override;
	};

	struct PhysicalDevice_impl {
		std::shared_ptr<Vulkan_impl_accesor> vulkan;


		vk::PhysicalDevice device;
		std::optional<vk::PhysicalDeviceProperties> props;
		std::optional<vk::PhysicalDeviceFeatures> features;

		void makeProps();
	};

	struct PhysicalDevices_impl {
		std::shared_ptr<Vulkan_impl_accesor> vulkan;

		std::vector<vk::PhysicalDevice> devices = {};
	};

	struct Surface_impl {
		std::shared_ptr<Vulkan_impl_accesor> vulkan;

		vk::SurfaceKHR surface;
		void* window;

		~Surface_impl();
	};

	struct LogicalDevice_impl {
		std::shared_ptr<PhysicalDevice_impl> parentDevice;
		std::shared_ptr<Surface_impl> parentSurface;

		vk::Device device;
		uint32_t graphicsFamily = 0, presentFamily = 0;
		vk::SurfaceFormatKHR surfaceFormat;
		vk::PresentModeKHR presentMode;
		vk::Extent2D extent;
		uint32_t imageCount;
		vk::SurfaceTransformFlagBitsKHR preTransform;

		~LogicalDevice_impl();
	};

	struct CommandManager_impl {
		std::shared_ptr<LogicalDevice_impl> device;

		vk::Queue graphicsQueue, presentQueuea;
	};

	struct Swapchain_impl {
		std::shared_ptr<LogicalDevice_impl> device;

		vk::SwapchainKHR swapChain;
		std::vector<vk::Image> swapChainImages;
		vk::Format swapChainImageFormat;
		vk::Extent2D swapChainExtent;

		~Swapchain_impl();
	};

	struct View_impl {
		std::shared_ptr<LogicalDevice_impl> device;
		std::vector<vk::ImageView> swapChainImageViews;

		~View_impl();
	};

	/* struct Viewport_impl {
		std::shared_ptr<Swapchain_impl> swapchain;
	}; */

	struct CreateInfo {
		vk::PipelineLayoutCreateInfo pipelineLayout;
		vk::PipelineVertexInputStateCreateInfo vertexInput;
		vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
		std::vector<vk::Viewport> viewports;
		std::vector<vk::Rect2D> scissors;
		vk::PipelineRasterizationStateCreateInfo rasterization;
		vk::PipelineMultisampleStateCreateInfo multisample;
		vk::PipelineColorBlendStateCreateInfo colorBlend;

		std::map<std::string, std::any> pool;
	};

	struct PipelineLayout_impl {
		vk::PipelineLayout pipelineLayout;
		std::shared_ptr<CreateInfo> info;
		std::shared_ptr<LogicalDevice_impl> device;

		~PipelineLayout_impl();
	};

	struct ShaderStage_impl {
		vk::PipelineShaderStageCreateInfo stage;
	};

	struct Shader_impl {
		std::shared_ptr<LogicalDevice_impl> device;
		vk::ShaderModule shaderModule;

		~Shader_impl();
	};

	struct RenderPass_impl {
		std::shared_ptr<LogicalDevice_impl> device;
		vk::RenderPass renderPass;

		~RenderPass_impl();
	};

	namespace helper {
		struct QueueFamilyIndices {
			std::optional<uint32_t> graphicsFamily;
			std::optional<uint32_t> presentFamily;

			bool isComplete();
		};

		QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device, vk::SurfaceKHR surface);
		void DestroyDebugUtilsMessengerEXT(vk::Instance& instance, vk::DebugUtilsMessengerEXT& debugMessenger, const VkAllocationCallbacks* pAllocator);
	}
}