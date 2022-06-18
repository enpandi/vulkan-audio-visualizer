#include "SurfaceInfo.hpp"

#include <algorithm>
#include <stdexcept>
#include <tuple>

namespace av {
	SurfaceInfo::SurfaceInfo(
		vk::raii::SurfaceKHR const &surface,
		GraphicsDevice const &gpu,
		std::tuple<size_t, size_t> const &framebuffer_size
	)
		: surface_capabilities{gpu.physical_device.getSurfaceCapabilitiesKHR(*surface)}
		, surface_format{choose_surface_format(surface, gpu)}
		, present_mode{choose_present_mode(surface, gpu)}
		, extent{get_extent(surface_capabilities, framebuffer_size)} {}

	vk::SurfaceFormatKHR SurfaceInfo::choose_surface_format(
		vk::raii::SurfaceKHR const &surface,
		GraphicsDevice const &gpu
	) {
		auto surface_formats = gpu.physical_device.getSurfaceFormatsKHR(*surface);
		if (surface_formats.empty())
			throw std::runtime_error("Vulkan failed to find surface formats");
		for (auto &&surface_format : surface_formats)
			if (surface_format.format == vk::Format::eB8G8R8A8Srgb
			    && surface_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
				return surface_format;
		return surface_formats.front(); // try different formats
	}

	vk::PresentModeKHR SurfaceInfo::choose_present_mode(
		vk::raii::SurfaceKHR const &surface,
		GraphicsDevice const &gpu
	) {
		auto present_modes = gpu.physical_device.getSurfacePresentModesKHR(*surface);
		if (present_modes.empty())
			throw std::runtime_error("Vulkan failed to find present modes");
		for (vk::PresentModeKHR const &present_mode : present_modes)
			if (present_mode == vk::PresentModeKHR::eMailbox)
				return present_mode;
		return vk::PresentModeKHR::eFifo; // try different modes
	}

	vk::Extent2D SurfaceInfo::get_extent(
		vk::SurfaceCapabilitiesKHR const &surface_capabilities,
		std::tuple<size_t, size_t> const &framebuffer_size
	) {
		if (surface_capabilities.currentExtent == vk::Extent2D{
			.width = 0xFFFFFFFF,
			.height = 0xFFFFFFFF,
			// https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VkSurfaceCapabilitiesKHR.html
			// currentExtent is the current WIDTH and HEIGHT of the surface, or the special value (0xFFFFFFFF, 0xFFFFFFFF)
			// indicating that the surface size will be determined by the extent of a swapchain targeting the surface.
		})
			return surface_capabilities.currentExtent;
		auto [width, height] = framebuffer_size;
		return {
			.width = std::clamp<uint32_t>(width, surface_capabilities.minImageExtent.width,
			                              surface_capabilities.maxImageExtent.width),
			.height = std::clamp<uint32_t>(height, surface_capabilities.minImageExtent.height,
			                               surface_capabilities.maxImageExtent.height),
		};
	}
} // av
