#ifndef AUDIO_VISUALIZER_GRAPHICSSTATE_HPP
#define AUDIO_VISUALIZER_GRAPHICSSTATE_HPP

#include "GraphicsDevice.hpp"
#include "SurfaceInfo.hpp"
#include "Framebuffer.hpp"
#include "graphics_headers.hpp"
#include <tuple>

namespace av {
	struct GraphicsState {
		GraphicsState(
			vk::raii::SurfaceKHR const &,
			GraphicsDevice const &,
			std::tuple<size_t, size_t> const &framebuffer_size
		);

		void recreate(
			vk::raii::SurfaceKHR const &,
			GraphicsDevice const &,
			std::tuple<size_t, size_t> const &framebuffer_size
		);

		vk::raii::ShaderModule const vertex_shader_module;
		vk::raii::ShaderModule const fragment_shader_module;
		vk::raii::PipelineLayout const pipeline_layout;
		SurfaceInfo const &surface_info{_surface_info};
		vk::raii::SwapchainKHR const &swapchain{_swapchain};
		vk::raii::RenderPass const &render_pass{_render_pass};
		vk::raii::Pipeline const &pipeline{_pipeline};
		Framebuffers const &framebuffers{_framebuffers};

	private:
		SurfaceInfo _surface_info;
		vk::raii::SwapchainKHR _swapchain;
		vk::raii::RenderPass _render_pass;
		vk::raii::Pipeline _pipeline;
		Framebuffers _framebuffers;

		static vk::raii::ShaderModule create_shader_module(std::string const &file_name, GraphicsDevice const &);

		static vk::raii::PipelineLayout create_pipeline_layout(GraphicsDevice const &);

		static vk::raii::SwapchainKHR create_swapchain(
			vk::raii::SurfaceKHR const &,
			GraphicsDevice const &,
			SurfaceInfo const &,
			vk::raii::SwapchainKHR const &old_swapchain
		);

		static vk::raii::RenderPass create_render_pass(GraphicsDevice const &, SurfaceInfo const &);

		static vk::raii::Pipeline create_pipeline(
			GraphicsDevice const &,
			vk::raii::ShaderModule const &vertex_shader_module,
			vk::raii::ShaderModule const &fragment_shader_module,
			vk::raii::PipelineLayout const &pipeline_layout,
			SurfaceInfo const &,
			vk::raii::RenderPass const &
		);
	};
} // av

#endif //AUDIO_VISUALIZER_GRAPHICSSTATE_HPP
