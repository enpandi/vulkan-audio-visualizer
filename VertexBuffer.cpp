#include "VertexBuffer.hpp"

#include "constants.hpp"
#include <algorithm>
#include <tuple>
#include <vector>

namespace av {
	VertexBuffer::VertexBuffer(
		size_t num_vertices,
		size_t num_indices,
		GraphicsDevice const &gpu,
		Allocator const &allocator
	) : VertexBuffer{
		num_vertices, num_indices,
		gpu, allocator,
		create_mapped_buffer(num_vertices, num_indices, allocator)
	} {}


	VertexBuffer::~VertexBuffer() {
		allocator.freeMemory(allocation);
	}

	/*
	void VertexBuffer::set_vertices(std::vector<Vertex> const &vertices) {
		num_vertices = vertices.size();
		std::ranges::copy(vertices, vertex_data.data());
		// todo staging buffer and custom allocator to use the mapped pointer directly
	}
	*/

	void VertexBuffer::bind_and_draw(const vk::raii::CommandBuffer &command_buffer) const {
		command_buffer.bindVertexBuffers(0, {*buffer}, {0});
		command_buffer.bindIndexBuffer(*buffer, num_vertices * sizeof(Vertex), constants::vk_index_type);
//		command_buffer.draw(num_vertices, 1, 0, 0);
		command_buffer.drawIndexed(num_indices, 1, 0, 0, 0);
		// todo index buffer
	}

	VertexBuffer::VertexBuffer(
		size_t num_vertices,
		size_t num_indices,
		GraphicsDevice const &gpu,
		Allocator const &allocator,
		std::tuple<vk::Buffer, vma::Allocation, void *> const &buffer_objects
	)
		: num_vertices{num_vertices}
		, num_indices{num_indices}
		, allocator{allocator}
		, buffer{gpu.device, std::get<vk::Buffer>(buffer_objects)}
		, allocation{std::get<vma::Allocation>(buffer_objects)}
		, _vertex_data{static_cast<Vertex *const>(std::get<void *>(buffer_objects)), num_vertices}
		, _index_data{
			static_cast<constants::index_t *const>(std::get<void *>(buffer_objects) + num_vertices * sizeof(Vertex)),
			num_indices
		} {}

	std::tuple<vk::Buffer, vma::Allocation, void *> VertexBuffer::create_mapped_buffer(
		size_t num_vertices,
		size_t num_indices,
		Allocator const &allocator
	) {
		// todo check for alignment issues
		vk::BufferCreateInfo buffer_create_info{
			.size = num_vertices * sizeof(Vertex) + num_indices * sizeof(constants::index_t),
			.usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer,
			.sharingMode = vk::SharingMode::eExclusive,
		};
		vma::AllocationCreateInfo allocation_create_info{
			.flags = vma::AllocationCreateFlagBits::eHostAccessSequentialWrite | vma::AllocationCreateFlagBits::eMapped,
			.usage = vma::MemoryUsage::eAutoPreferHost, // base address register memory causes mouse lag?
		};
		vma::AllocationInfo allocation_info;
		auto buffer_and_allocation = allocator.createBuffer(
			buffer_create_info, allocation_create_info, &allocation_info);
//		std::string vertex_buffer_memory_type = to_string(vk::MemoryPropertyFlags(allocation_info.memoryType));
		return std::make_tuple(buffer_and_allocation.first, buffer_and_allocation.second, allocation_info.pMappedData);
	}
} // av
