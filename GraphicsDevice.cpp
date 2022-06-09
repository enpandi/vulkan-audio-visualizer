#include "GraphicsDevice.hpp"

#include "constants.hpp"
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

av::GraphicsDevice::GraphicsDevice(
	vk::raii::Instance const &instance,
	vk::raii::SurfaceKHR const &surface
)
	: physical_device{choose_physical_device(instance, surface)}
	, queue_family_indices{*QueueFamilyIndices::get_queue_family_indices(physical_device, surface)}
	, device{create_device(physical_device, queue_family_indices)}
	, graphics_queue{device, queue_family_indices.graphics, 0}
	, present_queue{device, queue_family_indices.present, 0}
	, graphics_command_pool{create_graphics_command_pool(device, queue_family_indices.graphics)} {}

std::optional<av::GraphicsDevice::QueueFamilyIndices> av::GraphicsDevice::QueueFamilyIndices::get_queue_family_indices(
	vk::raii::PhysicalDevice const &physical_device,
	vk::raii::SurfaceKHR const &surface
) {
	std::optional<uint32_t> graphics_queue_family_index, present_queue_family_index;
	auto queue_families_properties = physical_device.getQueueFamilyProperties();
	for (uint32_t queue_family_index = 0; queue_family_index < queue_families_properties.size(); ++queue_family_index) {
		bool supports_graphics = static_cast<bool>(queue_families_properties[queue_family_index].queueFlags &
		                                           vk::QueueFlagBits::eGraphics);
		bool supports_present = physical_device.getSurfaceSupportKHR(queue_family_index, *surface);
		if (supports_graphics && supports_present) {
			graphics_queue_family_index = present_queue_family_index = queue_family_index;
			break;
		}
		if (supports_graphics && !graphics_queue_family_index.has_value())
			graphics_queue_family_index = queue_family_index;
		if (supports_present && !present_queue_family_index.has_value())
			present_queue_family_index = queue_family_index;
	}
	if (graphics_queue_family_index.has_value() && present_queue_family_index.has_value())
		return QueueFamilyIndices{
			.graphics= *graphics_queue_family_index,
			.present = *present_queue_family_index,
		};
	else return std::nullopt;
}


bool av::GraphicsDevice::physical_device_is_compatible(
	vk::raii::PhysicalDevice const &physical_device,
	vk::raii::SurfaceKHR const &surface
) {
	{
		auto extensions_properties = physical_device.enumerateDeviceExtensionProperties();
		bool supports_all_extensions = std::ranges::all_of(
			av::constants::DEVICE_EXTENSIONS,
			[&](std::string const &device_extension) {
				return std::ranges::any_of(
					extensions_properties,
					[&](vk::ExtensionProperties const &extension_properties) {
						return device_extension == extension_properties.extensionName;
					});
//					extensions_properties | std::views::transform(&vk::ExtensionProperties::extensionName),
//					std::bind(std::equal_to<std::string>(), device_extension, std::placeholders::_1));
			});
		if (!supports_all_extensions) return false;
	}
	if (!QueueFamilyIndices::get_queue_family_indices(physical_device, surface).has_value())
		return false;
	if (physical_device.getSurfaceFormatsKHR(*surface).empty())
		return false;
	if (physical_device.getSurfacePresentModesKHR(*surface).empty())
		return false;
	return true;
}

vk::raii::PhysicalDevice av::GraphicsDevice::choose_physical_device(
	vk::raii::Instance const &instance,
	vk::raii::SurfaceKHR const &surface
) {
	vk::raii::PhysicalDevices physical_devices(instance);
	auto usable_physical_device_indices = std::views::iota(0u, physical_devices.size())
	                                      | std::views::filter(
		[&](uint32_t const &physical_device_index) {
			return physical_device_is_compatible(physical_devices[physical_device_index], surface);
		});
	if (!usable_physical_device_indices)
		throw std::runtime_error("vulkan failed to find usable physical devices.");
	for (uint32_t physical_device_index : usable_physical_device_indices)
		if (physical_devices[physical_device_index].getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
			return std::move(physical_devices[physical_device_index]);
	return std::move(physical_devices.front());
}

vk::raii::Device av::GraphicsDevice::create_device(
	vk::raii::PhysicalDevice const &physical_device,
	av::GraphicsDevice::QueueFamilyIndices const &queue_family_indices
) {
	std::vector<vk::DeviceQueueCreateInfo> device_queue_create_infos;
	float queue_priority = 0.0f;
	device_queue_create_infos.push_back(
		{
			.queueFamilyIndex=queue_family_indices.graphics,
			.queueCount=1,
			.pQueuePriorities=&queue_priority,
		});
	if (queue_family_indices.graphics != queue_family_indices.present)
		device_queue_create_infos.push_back(
			{
				.queueFamilyIndex = queue_family_indices.present,
				.queueCount = 1,
				.pQueuePriorities = &queue_priority,
			});
	vk::PhysicalDeviceFeatures enabled_features;
	vk::DeviceCreateInfo device_create_info{
		.queueCreateInfoCount = static_cast<uint32_t>(device_queue_create_infos.size()),
		.pQueueCreateInfos = device_queue_create_infos.data(),
		.enabledLayerCount = static_cast<uint32_t>(av::constants::GLOBAL_LAYERS.size()),
		.ppEnabledLayerNames = av::constants::GLOBAL_LAYERS.data(),
		.enabledExtensionCount = av::constants::DEVICE_EXTENSIONS.size(),
		.ppEnabledExtensionNames = av::constants::DEVICE_EXTENSIONS.data(),
		.pEnabledFeatures = &enabled_features,
	};
	return {physical_device, device_create_info};
}

vk::raii::CommandPool av::GraphicsDevice::create_graphics_command_pool(
	vk::raii::Device const &device,
	uint32_t graphics_queue_family_index
) {
	vk::CommandPoolCreateInfo command_pool_create_info{
		.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		.queueFamilyIndex = graphics_queue_family_index,
	};
	return {device, command_pool_create_info};
}
