#ifndef AUDIO_VISUALIZER_VERTEXBUFFER_HPP
#define AUDIO_VISUALIZER_VERTEXBUFFER_HPP

#include "GraphicsDevice.hpp"
#include "Vertex.hpp"
#include "graphics_headers.hpp"
#include <tuple>
#include <vector>

#if 0
namespace av {
	class VertexBuffer {
		public:
		VertexBuffer(
			size_t num_vertices,
			GraphicsDevice const &,
			vma::Allocator const &
		);

		~VertexBuffer();

		void set_vertices(std::vector<Vertex> const &) ;

		void bind_and_draw(vk::raii::CommandBuffer const &) const;

	private:
		size_t num_vertices;
		vma::Allocator const &allocator;
		vk::raii::Buffer const buffer;
		vma::Allocation const allocation;
		Vertex *const _mapped_data;

		VertexBuffer(
			size_t num_vertices,
			vma::Allocator const &,
			GraphicsDevice const &,
			std::tuple<vk::Buffer, vma::Allocation, void *> const &
		);

		static std::tuple<vk::Buffer, vma::Allocation, void *> create_mapped_buffer(
			size_t num_vertices,
			vma::Allocator const &
		);
	};
}
#endif
namespace av {
	class VertexBuffer {
	public:
		VertexBuffer(
			size_t num_vertices,
			GraphicsDevice const &
		);
		void set_vertices(std::vector<Vertex> const &);
		void bind_and_draw(vk::raii::CommandBuffer const &) const;
		Vertex *const &mapped_data{_mapped_data};
	private:
		size_t num_vertices;
		vk::raii::Buffer const buffer;
		vk::raii::DeviceMemory const memory;
		Vertex *const _mapped_data;
	};
}

#endif //AUDIO_VISUALIZER_VERTEXBUFFER_HPP
