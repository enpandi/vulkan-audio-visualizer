#ifndef AUDIO_VISUALIZER_FRAMEBUFFER_HPP
#define AUDIO_VISUALIZER_FRAMEBUFFER_HPP

#include "Gpu.hpp"
#include "SurfaceInfo.hpp"
#include "graphics_headers.hpp"
#include <vector>

namespace av {
	class Framebuffer {
	public:
		Framebuffer(
			vk::raii::ImageView &&,
			vk::raii::Framebuffer &&
		);
		vk::raii::Framebuffer const &framebuffer{_framebuffer};
	private:
		vk::raii::ImageView image_view;
		vk::raii::Framebuffer _framebuffer;
	};

	class Framebuffers : std::vector<Framebuffer> {
	public:
		Framebuffers(
			GraphicsDevice const &,
			SurfaceInfo const &,
			vk::raii::SwapchainKHR const &,
			vk::raii::RenderPass const &
		);
		using std::vector<Framebuffer>::operator[];
	};
} // av

#endif //AUDIO_VISUALIZER_FRAMEBUFFER_HPP
