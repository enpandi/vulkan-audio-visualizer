#include "VertexBuffer.hpp"

#include "constants.hpp"
#include <algorithm>
#include <tuple>
#include <vector>

namespace av {
	VertexBuffer::VertexBuffer(
		size_t num_vertices,
		size_t num_indices,
		Gpu const &gpu,
		vma::Allocator const &allocator
	) : VertexBuffer{
		num_vertices, num_indices,
		gpu, allocator,
		create_mapped_buffer(num_vertices, num_indices, allocator)
	} {}


	VertexBuffer::~VertexBuffer() {
		_allocator.freeMemory(_allocation);
	}

	void VertexBuffer::bind_and_draw(const vk::raii::CommandBuffer &command_buffer) const {
		command_buffer.bindVertexBuffers(0, {*_buffer}, {0});
		command_buffer.bindIndexBuffer(*_buffer, _num_vertices * sizeof(Vertex), constants::vk_index_type);
		command_buffer.drawIndexed(_num_indices, 1, 0, 0, 0);
	}

	VertexBuffer::VertexBuffer(
		size_t num_vertices,
		size_t num_indices,
		Gpu const &gpu,
		vma::Allocator const &allocator,
		std::tuple<vk::Buffer, vma::Allocation, void *> const &buffer_objects
	)
		: _num_vertices{num_vertices}
		, _num_indices{num_indices}
		, _allocator{allocator}
		, _buffer{gpu.device, std::get<vk::Buffer>(buffer_objects)}
		, _allocation{std::get<vma::Allocation>(buffer_objects)}
		, _vertex_data{static_cast<Vertex *const>(std::get<void *>(buffer_objects)), num_vertices}
		, _index_data{
			reinterpret_cast<constants::index_t *const>(
				reinterpret_cast<Vertex *>(std::get<void *>(buffer_objects))
				+ num_vertices
			),
			num_indices
		} {}

	std::tuple<vk::Buffer, vma::Allocation, void *> VertexBuffer::create_mapped_buffer(
		size_t num_vertices,
		size_t num_indices,
		vma::Allocator const &allocator
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
