#ifndef AUDIO_VISUALIZER_RENDERER_HPP
#define AUDIO_VISUALIZER_RENDERER_HPP

#include "Allocator.hpp"
#include "Vertex.hpp"
#include "Window.hpp"
#include "GraphicsDevice.hpp"
#include "GraphicsState.hpp"
#include "VertexBuffer.hpp"
#include "Frame.hpp"
#include "graphics_headers.hpp"
#include <vector>

namespace av {
	class Renderer {
	public:

		explicit Renderer(size_t const &num_vertices, size_t const &num_indices);
//		Renderer(size_t const &num_vertices, bool const &ON_TOP, char const *TITLE, size_t WIDTH = 320, size_t HEIGHT = 240);

		~Renderer();

		// todo rule of 3? rule of 5? https://en.cppreference.com/w/cpp/language/rule_of_three

		[[nodiscard]] bool is_running() const;

		// upload vertices to the vertex buffer
		// todo this is probably not fast
		// todo try https://redd.it/aij7zp
//		void set_vertices(std::vector<Vertex> const &);

		void draw_frame();

	private:
		bool framebuffer_resized = false;

		vkfw::UniqueInstance const vkfw_instance;
		Window const window;

		vk::raii::Context const context;
		vk::raii::Instance const instance;
		vk::raii::SurfaceKHR const surface;
		GraphicsDevice const gpu;
		GraphicsState state;
		Allocator const allocator;
		VertexBuffer vertex_buffer;
	public:
		std::span<Vertex> const &vertex_data{vertex_buffer.vertex_data};
		std::span<constants::index_t> const &index_data{vertex_buffer.index_data};
	private:
		Frames const frames;

		uint32_t current_flight_frame = 0; // for multiple frames in flight

		static vk::raii::Instance create_instance(vk::raii::Context const &);

		void resize();

		void record_graphics_command_buffer(
			vk::raii::CommandBuffer const &command_buffer,
			vk::raii::Framebuffer const &framebuffer
		) const;
	};
} // av

#endif //AUDIO_VISUALIZER_RENDERER_HPP
