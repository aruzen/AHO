#ifdef _MSC_VER
#include "pch.h"
#endif
#include "../define.h"
#include "device.h"

#include "_pimpls.h"

#include "../exceptions.h"
#include "../debug.h"

#include "surface.h"

#include <set>
#include <cstdint> // Necessary for UINT32_MAX
#include <algorithm> // Necessary for std::clamp


bool vsl::_impl::helper::QueueFamilyIndices::isComplete() {
	return graphicsFamily.has_value() && presentFamily.has_value();
}

vsl::_impl::helper::QueueFamilyIndices  vsl::_impl::helper::findQueueFamilies(vk::PhysicalDevice device, vk::SurfaceKHR surface) {
	QueueFamilyIndices indices;
	std::vector<vk::QueueFamilyProperties> queueFamilies = device.getQueueFamilyProperties();
	int i = 0;
	for (const auto& queueFamily : queueFamilies) {
		if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) {
			indices.graphicsFamily = i;
		}
		vk::Bool32 presentSupport = device.getSurfaceSupportKHR(i, surface);
		if (presentSupport) {
			indices.presentFamily = i;
		}
		i++;
	}
	return indices;
}

using namespace vsl::_impl::helper;

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

bool checkDeviceExtensionSupport(vk::PhysicalDevice device) {
	std::vector<vk::ExtensionProperties> availableExtensions = device.enumerateDeviceExtensionProperties();

	std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

	for (const auto& extension : availableExtensions) {
		requiredExtensions.erase(extension.extensionName);
	}

	return requiredExtensions.empty();
}

struct SwapChainSupportDetails {
	vk::SurfaceCapabilitiesKHR capabilities {};
	std::vector<vk::SurfaceFormatKHR> formats {};
	std::vector<vk::PresentModeKHR> presentModes {};
};

SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice device, vk::SurfaceKHR surface) {
	SwapChainSupportDetails details;
	details.capabilities = device.getSurfaceCapabilitiesKHR(surface);
	details.formats = device.getSurfaceFormatsKHR(surface);
	details.presentModes = device.getSurfacePresentModesKHR(surface);
	return details;
}

vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) {
	for (const auto& availableFormat : availableFormats) {
		if (availableFormat.format == vk::Format::eB8G8R8A8Srgb
			&& availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
			return availableFormat;
		}
	}
	return availableFormats[0];
}

vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes) {
	for (const auto& availablePresentMode : availablePresentModes) {
		if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
			return availablePresentMode;
		}
	}
	return vk::PresentModeKHR::eFifo;
}

vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, GLFWwindow* window) {
	if (capabilities.currentExtent.width != UINT32_MAX) {
		return capabilities.currentExtent;
	}
	else {
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		vk::Extent2D actualExtent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)
		};

		actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

		return actualExtent;
	}
}

template<bool V>
std::shared_ptr<VSL_NAMESPACE::_impl::LogicalDevice_impl> VSL_NAMESPACE::_LogicalDeviceGraphicCreator<V>::create(VSL_NAMESPACE::PhysicalDevice device, VSL_NAMESPACE::Surface surface)
{
	std::shared_ptr<VSL_NAMESPACE::_impl::LogicalDevice_impl> _data = std::shared_ptr<VSL_NAMESPACE::_impl::LogicalDevice_impl>(new VSL_NAMESPACE::_impl::LogicalDevice_impl);

	QueueFamilyIndices indices = findQueueFamilies(device._data->device, surface._data->surface);
	bool extensionsSupported = checkDeviceExtensionSupport(device._data->device), swapChainAdequate = false;
	SwapChainSupportDetails swapChainSupport;
	if (extensionsSupported) {
		swapChainSupport = querySwapChainSupport(device._data->device, surface._data->surface);
		swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}

	if (!extensionsSupported && !swapChainAdequate && !indices.isComplete())
		throw VSL_NAMESPACE::exceptions::RuntimeException("CommandManager", "PhysicalDevices do not have requirements.", "VSL_NAMESPACE::CommandManager<V, C>::CommandManager");

	_data->surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
	_data->presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
	_data->extent = chooseSwapExtent(swapChainSupport.capabilities, (GLFWwindow*)surface._data->window);
	_data->imageCount = swapChainSupport.capabilities.minImageCount + 1;
	_data->preTransform = swapChainSupport.capabilities.currentTransform;
	if (swapChainSupport.capabilities.maxImageCount > 0 &&
		_data->imageCount > swapChainSupport.capabilities.maxImageCount) {
		_data->imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

	float queuePriority = 1.0f;
	for (uint32_t queueFamily : uniqueQueueFamilies) {
		vk::DeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	vk::PhysicalDeviceFeatures deviceFeatures;

	vk::DeviceCreateInfo createInfo;
	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.pEnabledFeatures = &deviceFeatures;
	createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	createInfo.ppEnabledExtensionNames = deviceExtensions.data();

	/*
	if constexpr (V) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(VSL_NAMESPACE::validationLayers.size());
		createInfo.ppEnabledLayerNames = VSL_NAMESPACE::validationLayers.data();
	}
	else {
		createInfo.enabledLayerCount = 0;
	}*/

	_data->device = device._data->device.createDevice(createInfo);
	_data->graphicsFamily = indices.graphicsFamily.value();
	_data->presentFamily = indices.presentFamily.value();

	return _data;
}

void VSL_NAMESPACE::_impl::PhysicalDevice_impl::makeProps()
{
	props = device.getProperties();
}

void VSL_NAMESPACE::_impl::PhysicalDevice_impl::makeMemProps()
{
	memProps = device.getMemoryProperties();
}

VSL_NAMESPACE::PhysicalDevice::PhysicalDevice(std::shared_ptr<_impl::PhysicalDevice_impl> _data) :_data(_data) {}

VSL_NAMESPACE::PhysicalDevice::~PhysicalDevice() {}

uint32_t VSL_NAMESPACE::PhysicalDevice::apiVersion()
{
	if (!_data->props.has_value())
		_data->makeProps();
	return _data->props.value().apiVersion;
}

uint32_t VSL_NAMESPACE::PhysicalDevice::driverVersion() {
	if (!_data->props.has_value())
		_data->makeProps();
	return _data->props.value().driverVersion;
}

uint32_t VSL_NAMESPACE::PhysicalDevice::vendorID() {
	if (!_data->props.has_value())
		_data->makeProps();
	return _data->props.value().vendorID;
}

uint32_t VSL_NAMESPACE::PhysicalDevice::deviceID() {
	if (!_data->props.has_value())
		_data->makeProps();
	return _data->props.value().deviceID;
}

VSL_NAMESPACE::PhysicalDeviceType::_PhysicalDeviceType VSL_NAMESPACE::PhysicalDevice::type() {
	if (!_data->props.has_value())
		_data->makeProps();
	return VSL_NAMESPACE::PhysicalDeviceType::getType((size_t)_data->props.value().deviceType);
}

std::string VSL_NAMESPACE::PhysicalDevice::name() {
	if (!_data->props.has_value())
		_data->makeProps();
	return _data->props.value().deviceName;
}

template<typename S>
VSL_NAMESPACE::PhysicalDevices<S>::PhysicalDevices(VSL_NAMESPACE::VulkanAccessor vulkan)
{
	_data = std::shared_ptr<VSL_NAMESPACE::_impl::PhysicalDevices_impl>(new VSL_NAMESPACE::_impl::PhysicalDevices_impl);
	_data->devices = vulkan._accessor->instance.enumeratePhysicalDevices();
	_data->vulkan = vulkan._accessor;
}

template<typename S>
VSL_NAMESPACE::PhysicalDevices<S>::~PhysicalDevices() {}

template<typename S>
std::vector<VSL_NAMESPACE::PhysicalDevice> VSL_NAMESPACE::PhysicalDevices<S>::get()
{
	std::vector<VSL_NAMESPACE::PhysicalDevice> devices;
	for (auto& d : _data->devices) {
		std::shared_ptr<VSL_NAMESPACE::_impl::PhysicalDevice_impl> nd(new VSL_NAMESPACE::_impl::PhysicalDevice_impl);
		nd->device = d;
		nd->vulkan = _data->vulkan;
		devices.push_back(VSL_NAMESPACE::PhysicalDevice(nd));
	}
	return devices;
}

template<typename S>
VSL_NAMESPACE::PhysicalDevice VSL_NAMESPACE::PhysicalDevices<S>::search()
{
	S s;
	int max_score = -1;
	std::shared_ptr<VSL_NAMESPACE::_impl::PhysicalDevice_impl> ret = nullptr;
	for (auto d : get()) {
		int score = s.benchmark(d);
		if (max_score < score)
			max_score = score;
			ret = d._data;
	}
	if (!ret)
		throw VSL_NAMESPACE::exceptions::RuntimeException("PhysicalDevices", "PhysicalDevices is not suitable.", "VSL_NAMESPACE::_PhysicalDevicesSercher::search");

	return VSL_NAMESPACE::PhysicalDevice(ret);
}

#ifdef VSL_NAMESPACE_TEST
template<typename S>
void VSL_NAMESPACE::PhysicalDevices<S>::test()
{
	std::vector<VSL_NAMESPACE::PhysicalDevice> devices;
	for (auto& d : _data->devices) {
		auto f = d.getFeatures();
		auto p = d.getProperties();
	}
	for (auto d : get()) {
		vsl::loggingln(d.name());
	}
}
#endif // VSL_NAMESPACE_TEST

VSL_NAMESPACE::PhysicalDeviceType::_PhysicalDeviceType VSL_NAMESPACE::PhysicalDeviceType::getType(int a)
{
	switch (a)
	{
	case 1:
		return VSL_NAMESPACE::PhysicalDeviceType::IntegratedGPU;
	case 2:
		return VSL_NAMESPACE::PhysicalDeviceType::DiscreteGPU;
	case 3:
		return VSL_NAMESPACE::PhysicalDeviceType::VirtualGPU;
	case 4:
		return VSL_NAMESPACE::PhysicalDeviceType::CPU;
	default:
		return VSL_NAMESPACE::PhysicalDeviceType::Other;
	}
}

int rateDeviceSuitability(vk::PhysicalDevice device) {
	auto deviceProperties = device.getProperties();
	auto deviceFeatures = device.getFeatures();

	int score = 0;

	if (deviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
		score += 1000;
	}

	score += deviceProperties.limits.maxImageDimension2D;

	if (!deviceFeatures.geometryShader) {
		return 0;
	}
	return score;
}

int VSL_NAMESPACE::_PhysicalDevicesSercher::benchmark(VSL_NAMESPACE::PhysicalDevice device)
{
	return rateDeviceSuitability(device._data->device);
}

template<typename C>
VSL_NAMESPACE::LogicalDevice<C>::LogicalDevice(VSL_NAMESPACE::PhysicalDevice _device, vsl::Surface _surface)
{
	C c;
	_data = c.create(_device, _surface);
	_data->parentDevice = _device._data;
	_data->parentSurface = _surface._data;
}

template<typename C>
VSL_NAMESPACE::LogicalDevice<C>::LogicalDevice(VSL_NAMESPACE::PhysicalDevice _device, std::shared_ptr<vsl::Surface> _surface) {
	C c;
	_data = c.create(_device, (vsl::Surface)(*_surface));
	_data->parentDevice = _device._data;
	_data->parentSurface = _surface->_data;
}

VSL_NAMESPACE::_impl::LogicalDevice_impl::~LogicalDevice_impl() {
	device.destroy();
}

template struct VSL_NAMESPACE::PhysicalDevices<VSL_NAMESPACE::_PhysicalDevicesSercher>;
template struct VSL_NAMESPACE::LogicalDevice<VSL_NAMESPACE::_LogicalDeviceGraphicCreator<true>>;
template struct VSL_NAMESPACE::LogicalDevice<VSL_NAMESPACE::_LogicalDeviceGraphicCreator<false>>;
