#ifndef AUDIO_VISUALIZER_VERTEXBUFFER_HPP
#define AUDIO_VISUALIZER_VERTEXBUFFER_HPP

#include "constants.hpp"
#include "Allocator.hpp"
#include "GraphicsDevice.hpp"
#include "Vertex.hpp"
#include "graphics_headers.hpp"
#include <span>
#include <tuple>
#include <vector>

namespace av {
	class VertexBuffer {
	public:
		VertexBuffer(
			size_t num_vertices,
			size_t num_indices,
			GraphicsDevice const &,
			Allocator const &
		);

		~VertexBuffer();

//		void set_vertices(std::vector<Vertex> const &);

		void bind_and_draw(vk::raii::CommandBuffer const &) const;

//		Vertex *const &mapped_data{_mapped_data};
		std::span<Vertex> const &vertex_data{_vertex_data};
		std::span<constants::index_t> const &index_data{_index_data};
	private:
		size_t num_vertices;
		size_t num_indices;
		Allocator const &allocator;
		vk::raii::Buffer const buffer;
		vma::Allocation const allocation;
		std::span<Vertex> const _vertex_data;
		std::span<constants::index_t> const _index_data;

		VertexBuffer(
			size_t num_vertices,
			size_t num_indices,
			GraphicsDevice const &,
			Allocator const &,
			std::tuple<vk::Buffer, vma::Allocation, void *> const &
		);

		static std::tuple<vk::Buffer, vma::Allocation, void *> create_mapped_buffer(
			size_t num_vertices,
			size_t num_indices,
			Allocator const &
		);
	};
} // av

#endif //AUDIO_VISUALIZER_VERTEXBUFFER_HPP
