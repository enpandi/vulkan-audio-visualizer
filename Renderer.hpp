#ifndef AUDIO_VISUALIZER_RENDERER_HPP
#define AUDIO_VISUALIZER_RENDERER_HPP

#include "Vertex.hpp"
#include "Window.hpp"
#include "Gpu.hpp"
#include "GraphicsState.hpp"
#include "VertexBuffer.hpp"
#include "Frame.hpp"
#include "graphics_headers.hpp"
#include <vector>

namespace av {
	class Renderer {
	public:
		Renderer(size_t const &num_vertices, size_t const &num_indices);
		~Renderer();
		// todo rule of 3? rule of 5? https://en.cppreference.com/w/cpp/language/rule_of_three
		[[nodiscard]] bool is_running() const;
		void draw_frame();

	private:
		vkfw::UniqueInstance const vkfw_instance;
		Window const window;
		vk::raii::Context const context;
		vk::raii::Instance const instance;
		vk::raii::SurfaceKHR const surface;
		Gpu const gpu;
		GraphicsState state;
		Frames const frames;
		VertexBuffer vertex_buffer;
		uint32_t current_flight_frame = 0; // for multiple frames in flight
		bool framebuffer_resized = false;

		static vk::raii::Instance create_instance(vk::raii::Context const &);
		void resize();
		void record_graphics_command_buffer(
			vk::raii::CommandBuffer const &command_buffer,
			vk::raii::Framebuffer const &framebuffer
		) const;
	};
} // av

#endif //AUDIO_VISUALIZER_RENDERER_HPP
