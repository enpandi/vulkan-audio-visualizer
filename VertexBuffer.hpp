#ifndef AUDIO_VISUALIZER_VERTEXBUFFER_HPP
#define AUDIO_VISUALIZER_VERTEXBUFFER_HPP

#include "constants.hpp"
#include "Gpu.hpp"
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
			Gpu const &,
			vma::Allocator const &
		);
		~VertexBuffer();
		void bind_and_draw(vk::raii::CommandBuffer const &) const;
		std::span<Vertex> const &vertex_data{_vertex_data};
		std::span<constants::index_t> const &index_data{_index_data};

	private:
		size_t _num_vertices;
		size_t _num_indices;
		vma::Allocator const &_allocator;
		vk::raii::Buffer const _buffer;
		vma::Allocation const _allocation;
		std::span<Vertex> const _vertex_data;
		std::span<constants::index_t> const _index_data;
		VertexBuffer(
			size_t num_vertices,
			size_t num_indices,
			Gpu const &,
			vma::Allocator const &,
			std::tuple<vk::Buffer, vma::Allocation, void *> const &
		);
		static std::tuple<vk::Buffer, vma::Allocation, void *> create_mapped_buffer(
			size_t num_vertices,
			size_t num_indices,
			vma::Allocator const &
		);
	};
} // av

#endif //AUDIO_VISUALIZER_VERTEXBUFFER_HPP
