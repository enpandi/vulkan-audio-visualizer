#ifndef AUDIO_VISUALIZER_SURFACEINFO_HPP
#define AUDIO_VISUALIZER_SURFACEINFO_HPP

#include "Gpu.hpp"
#include "graphics_headers.hpp"
#include <tuple>

namespace av {
	struct SurfaceInfo {
		SurfaceInfo(
			vk::raii::SurfaceKHR const &,
			Gpu const &,
			std::tuple<size_t, size_t> const &framebuffer_size
		);
		vk::SurfaceCapabilitiesKHR const surface_capabilities;
		vk::SurfaceFormatKHR const surface_format;
		vk::PresentModeKHR const present_mode;
		vk::Extent2D const extent;

	private:
		static vk::SurfaceFormatKHR choose_surface_format(
			vk::raii::SurfaceKHR const &,
			Gpu const &
		);
		static vk::PresentModeKHR choose_present_mode(
			vk::raii::SurfaceKHR const &,
			Gpu const &
		);
		static vk::Extent2D get_extent(
			vk::SurfaceCapabilitiesKHR const &,
			std::tuple<size_t, size_t> const &framebuffer_size
		);
	};
} // av

#endif //AUDIO_VISUALIZER_SURFACEINFO_HPP
