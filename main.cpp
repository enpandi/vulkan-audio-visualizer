#include <cstdint>
#include <iostream>
#include <array>
#include <ranges>
#include <algorithm>
#include <utility>
#include <functional>

#define VULKAN_HPP_NO_CONSTRUCTORS
#define VKFW_NO_STRUCT_CONSTRUCTORS
#include <vkfw/vkfw.hpp>
#include <vulkan/vulkan_raii.hpp>

// todo disable stuff in release mode, for now assume debug mode always
static std::array const vk_global_layers = {"VK_LAYER_KHRONOS_validation"}; // todo is there a macro for this, or:
static std::array const vk_device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME}; // todo is there a vulkan-hpp constant for this
static std::string const application_name = "audio visualizer";

struct QueueFamilyIndices {
	std::optional<uint32_t> graphics_queue_family_index;
	std::optional<uint32_t> present_queue_family_index;

	QueueFamilyIndices(vk::raii::PhysicalDevice const &physical_device, vk::raii::SurfaceKHR const &surface) {
		std::vector<vk::QueueFamilyProperties> queue_family_properties = physical_device.getQueueFamilyProperties();
		auto is_graphics_supported = [&](uint32_t queue_family_index) {
			return static_cast<bool>(queue_family_properties[queue_family_index].queueFlags & vk::QueueFlagBits::eGraphics);
		};
		auto is_surface_supported = [&](uint32_t queue_family_index) {
			return static_cast<bool>(physical_device.getSurfaceSupportKHR(queue_family_index, *surface));
		};
		auto usable_queue_families = std::views::iota(0u, queue_family_properties.size())
		                             | std::views::filter(is_graphics_supported)
		                             | std::views::filter(is_surface_supported);
		if (!std::ranges::empty(usable_queue_families)) {
			graphics_queue_family_index = usable_queue_families.front();
			present_queue_family_index = usable_queue_families.front();
		} else {
			auto graphics_queue_families = std::views::iota(0u, queue_family_properties.size())
			                               | std::views::filter(is_graphics_supported);
			if (!std::ranges::empty(graphics_queue_families))
				graphics_queue_family_index = graphics_queue_families.front();

			auto present_queue_families = std::views::iota(0u, queue_family_properties.size())
			                              | std::views::filter(is_surface_supported);
			if (!std::ranges::empty(present_queue_families))
				present_queue_family_index = present_queue_families.front();
		}
	}

	[[nodiscard]] bool has_all_indices() const {
		return graphics_queue_family_index.has_value() && present_queue_family_index.has_value();
	}
};

struct SurfaceInformation {
	std::vector<vk::SurfaceFormatKHR> surface_formats;
	vk::SurfaceCapabilitiesKHR surface_capabilities;
	std::vector<vk::PresentModeKHR> present_modes;

	SurfaceInformation(vk::raii::PhysicalDevice const &physical_device, vk::raii::SurfaceKHR const &surface) :
		surface_formats{
			physical_device.getSurfaceFormatsKHR(*surface)
		},
		surface_capabilities{
			physical_device.getSurfaceCapabilitiesKHR(*surface)
		},
		present_modes{
			physical_device.getSurfacePresentModesKHR(*surface)
		} {}

	// todo for these functions, & or no & ?
	[[nodiscard]] vk::SurfaceFormatKHR choose_surface_format() const {
		if (surface_formats.empty())
			throw std::runtime_error("there weren't any surface formats to choose from.");
		// should this be std::out_of_range ?
		for (vk::SurfaceFormatKHR const &surface_format: surface_formats) {
			if (surface_format.format == vk::Format::eB8G8R8A8Srgb
			    && surface_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
				return surface_format;
		}
		return surface_formats.front();
		// todo mess around with this
	}

	// vkfw::UniqueWindow vs vkfw::Window ?
	[[nodiscard]] vk::Extent2D choose_extent(vkfw::UniqueWindow const &window) const {
		if (surface_capabilities.currentExtent == vk::Extent2D{
			.width = 0xFFFFFFFF,
			.height = 0xFFFFFFFF,
			// https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VkSurfaceCapabilitiesKHR.html
			// currentExtent is the current width and height of the surface, or the special value (0xFFFFFFFF, 0xFFFFFFFF)
			// indicating that the surface size will be determined by the extent of a swapchain targeting the surface.
		})
			return surface_capabilities.currentExtent;
		auto[width, height] = window->getFramebufferSize();
		return {
			.width = std::clamp<uint32_t>(width, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width),
			.height = std::clamp<uint32_t>(height, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height),
		};
	}

	[[nodiscard]] vk::PresentModeKHR choose_present_mode() const {
		if (present_modes.empty())
			throw std::runtime_error("there weren't any present modes to choose from.");
		// should this be std::out_of_range ?
		for (vk::PresentModeKHR const &present_mode: present_modes) {
			if (present_mode == vk::PresentModeKHR::eMailbox)
				return present_mode;
		}
		return vk::PresentModeKHR::eFifo;
		// todo mess around with this e.g. immediate, fifo relaxed
		// todo prefer fifo for low energy consumption
	}
};

vk::raii::Instance vk_create_instance(vk::raii::Context const &context) {
	vk::ApplicationInfo application_info = {
		.pApplicationName = application_name.c_str(),
		.apiVersion = VK_API_VERSION_1_1,
	};
	std::span<char const *const> glfw_extensions = vkfw::getRequiredInstanceExtensions();
	vk::InstanceCreateInfo instance_create_info = {
		.pApplicationInfo = &application_info,
		.enabledLayerCount = vk_global_layers.size(),
		.ppEnabledLayerNames = vk_global_layers.data(),
		.enabledExtensionCount = static_cast<uint32_t>(glfw_extensions.size()),
		.ppEnabledExtensionNames = glfw_extensions.data(),
	};
	return {context, instance_create_info};
}

vk::raii::PhysicalDevice vk_choose_physical_device(vk::raii::Instance const &instance, vk::raii::SurfaceKHR const &surface) {
	vk::raii::PhysicalDevices physical_devices(instance);
	auto usable_physical_devices = std::views::all(physical_devices)
	                               | std::views::filter(
		[&](vk::raii::PhysicalDevice const &physical_device) {
			bool queue_families_present = QueueFamilyIndices(physical_device, surface).has_all_indices();
			std::vector<vk::ExtensionProperties> extensions_properties = physical_device.enumerateDeviceExtensionProperties();
			bool device_extensions_supported = std::ranges::all_of(
				vk_device_extensions,
				[&](std::string const &device_extension) {
					return std::ranges::any_of(
						extensions_properties,
						[&](vk::ExtensionProperties const &extension_properties) {
							return device_extension == extension_properties.extensionName;
						});
				});
			SurfaceInformation surface_information(physical_device, surface);
			bool surface_ok = !surface_information.surface_formats.empty() && !surface_information.present_modes.empty();
			return queue_families_present && device_extensions_supported && surface_ok;
		});
	if (std::ranges::empty(usable_physical_devices))
		throw std::runtime_error("vulkan failed to find usable physical devices.");
	return std::move(*std::ranges::max_element(
		usable_physical_devices,
		[](vk::raii::PhysicalDevice const &physical_device_1, vk::raii::PhysicalDevice const &physical_device_2) {
			return (physical_device_1.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
			       <
			       (physical_device_2.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu);
		}));
}

vk::raii::Device vk_create_device(vk::raii::PhysicalDevice const &physical_device) {
	std::vector<vk::QueueFamilyProperties> queue_families_properties = physical_device.getQueueFamilyProperties();
	uint32_t graphics_queue_family_index = std::distance(
		queue_families_properties.begin(),
		std::ranges::find_if(
			queue_families_properties,
			[](vk::QueueFamilyProperties const &queue_family_properties) {
				return static_cast<bool>(queue_family_properties.queueFlags & vk::QueueFlagBits::eGraphics);
			}));
	float queue_priority = 0.0f;
	vk::DeviceQueueCreateInfo queue_create_info = {
		.queueFamilyIndex = graphics_queue_family_index,
		.queueCount = 1,
		.pQueuePriorities = &queue_priority,
	};
	vk::PhysicalDeviceFeatures enabled_features;
	vk::DeviceCreateInfo device_create_info = {
		.queueCreateInfoCount = 1,
		.pQueueCreateInfos = &queue_create_info,
		.enabledLayerCount = vk_global_layers.size(),
		.ppEnabledLayerNames = vk_global_layers.data(),
		.enabledExtensionCount = vk_device_extensions.size(),
		.ppEnabledExtensionNames = vk_device_extensions.data(),
		.pEnabledFeatures = &enabled_features,
	};
	return {physical_device, device_create_info};
}

vk::raii::SwapchainKHR vk_create_swapchain(
	vk::raii::Device const &device,
	vk::raii::PhysicalDevice const &physical_device, vk::raii::SurfaceKHR const &surface, vkfw::UniqueWindow const &window
) {
	SurfaceInformation surface_information(physical_device, surface);
	vk::SurfaceFormatKHR surface_format = surface_information.choose_surface_format();
	QueueFamilyIndices queue_family_indices(physical_device, surface);
	std::array queue_family_index_array = {*queue_family_indices.graphics_queue_family_index, *queue_family_indices.present_queue_family_index};
	vk::SwapchainCreateInfoKHR swapchain_create_info{
		.surface = *surface,
		.minImageCount = surface_information.surface_capabilities.maxImageCount == 0
		                 ? surface_information.surface_capabilities.minImageCount + 1
		                 : std::min(surface_information.surface_capabilities.minImageCount + 1, surface_information.surface_capabilities.maxImageCount),
		.imageFormat = surface_format.format,
		.imageColorSpace = surface_format.colorSpace,
		.imageExtent = surface_information.choose_extent(window),
		.imageArrayLayers = 1,
		.imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
		.imageSharingMode = *queue_family_indices.graphics_queue_family_index == *queue_family_indices.present_queue_family_index ?
		                    vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent,
		.queueFamilyIndexCount = queue_family_index_array.size(),
		.pQueueFamilyIndices = queue_family_index_array.data(),
//		.preTransform = surface_information.surface_capabilities.currentTransform, // default
		.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque, // default
		.presentMode = surface_information.choose_present_mode(),
		.clipped = true,
//		.oldSwapchain = VK_NULL_HANDLE, // ???
	};
	return {device, swapchain_create_info};
}

std::vector<vk::raii::ImageView> vk_create_image_views(std::vector<VkImage> const &images, vk::raii::Device const &device, vk::Format const &format) {
	std::vector<vk::raii::ImageView> image_views;
	image_views.reserve(images.size());
	for (VkImage const &image: images) {
		vk::ImageViewCreateInfo image_view_create_info = {
			.image = image,
			.viewType = vk::ImageViewType::e2D,
			.format = format,
			.components = vk::ComponentMapping{
				.r =vk::ComponentSwizzle::eIdentity,
				.g =vk::ComponentSwizzle::eIdentity,
				.b =vk::ComponentSwizzle::eIdentity,
				.a =vk::ComponentSwizzle::eIdentity,
			}, // todo mess with the color swizzling here
			.subresourceRange = {
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		};
		image_views.emplace_back(device, image_view_create_info);
	}
	return image_views;
}



int main() {
	vkfw::setErrorCallback(
		[](int error_code, char const *const description) {
			std::cerr << "glfw error callback: error code " << error_code << ", " << description << std::endl;
		}
	);
	try {
		auto const vkfw_instance = vkfw::initUnique();
		vkfw::UniqueWindow const vkfw_window = vkfw::createWindowUnique(640, 480, "window name", vkfw::WindowHints{.resizable = true});
		vk::raii::Context const vk_context;
		vk::raii::Instance const vk_instance = vk_create_instance(vk_context);
		// mixing normal and unique and raii is weird
		vk::raii::SurfaceKHR const vk_surface(vk_instance, vkfw::createWindowSurface(*vk_instance, vkfw_window.get()));
		vk::raii::PhysicalDevice const vk_physical_device = vk_choose_physical_device(vk_instance, vk_surface);
		vk::raii::Device const vk_device = vk_create_device(vk_physical_device);
//		QueueFamilyIndices queue_family_indices(vk_physical_device, vk_surface);
//		vk::raii::Queue const vk_graphics_queue(vk_device, *queue_family_indices.graphics_queue_family_index, 0);
//		vk::raii::Queue const vk_present_queue(vk_device, *queue_family_indices.present_queue_family_index, 0);
		vk::raii::SwapchainKHR const vk_swapchain = vk_create_swapchain(vk_device, vk_physical_device, vk_surface, vkfw_window); // todo refactor?
		// todo "we'll need the format and extent in future chapters" (https://vulkan-tutorial.com/en/Drawing_a_triangle/Presentation/Swap_chain)
		std::vector<VkImage> const vk_images = vk_swapchain.getImages(); // VkImage is a handle, works differently wrt vk::Image
		vk::Format const vk_format = SurfaceInformation(vk_physical_device, vk_surface).choose_surface_format().format; // todo i have a lot of redundancy
		std::vector<vk::raii::ImageView> const vk_image_views = vk_create_image_views(vk_images, vk_device, vk_format);

		// todo some of the consts must go
	} catch (std::system_error &err) {
		std::cerr << "std::system_error: code " << err.code() << ": " << err.what() << std::endl;
		std::exit(EXIT_FAILURE);
	} catch (std::exception &err) {
		std::cerr << "std::exception: " << err.what() << std::endl;
		std::exit(EXIT_FAILURE);
	} catch (...) {
		std::cerr << "unknown error" << std::endl;
		std::exit(EXIT_FAILURE);
	}
	// todo i have no idea what to catch
}
