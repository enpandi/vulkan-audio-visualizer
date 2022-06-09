#include "VertexBuffer.hpp"

#include <algorithm>
#include <tuple>
#include <vector>

av::VertexBuffer::VertexBuffer(
	size_t num_vertices,
	GraphicsDevice const &gpu,
	vma::Allocator const &allocator
)
	: VertexBuffer(num_vertices, allocator, gpu, create_mapped_buffer(num_vertices, allocator)) {}

av::VertexBuffer::~VertexBuffer() {
	allocator.freeMemory(allocation);
}

void av::VertexBuffer::set_vertices(std::vector<Vertex> const &vertices) {
	num_vertices = vertices.size();
	std::copy(vertices.begin(), vertices.end(), mapped_data);
	// todo staging buffer and custom allocator to use the mapped pointer directly
}

void av::VertexBuffer::bind_and_draw(const vk::raii::CommandBuffer &command_buffer) const {
	command_buffer.bindVertexBuffers(0, {*buffer}, {0});
	command_buffer.draw(num_vertices, 1, 0, 0);
	// todo index buffer
}

av::VertexBuffer::VertexBuffer(
	size_t num_vertices,
	vma::Allocator const &allocator,
	GraphicsDevice const &gpu,
	std::tuple<vk::Buffer, vma::Allocation, void *> const &buffer_objects
)
	: num_vertices{num_vertices}
	, allocator{allocator}
	, buffer{gpu.device, std::get<vk::Buffer>(buffer_objects)}
	, allocation{std::get<vma::Allocation>(buffer_objects)}
	, mapped_data{static_cast<Vertex *const>(std::get<void *>(buffer_objects))} {}

std::tuple<vk::Buffer, vma::Allocation, void *> av::VertexBuffer::create_mapped_buffer(
	size_t num_vertices,
	vma::Allocator const &allocator
) {
	vk::BufferCreateInfo buffer_create_info{
		.size = num_vertices * sizeof(Vertex),
		.usage = vk::BufferUsageFlagBits::eVertexBuffer,
		.sharingMode = vk::SharingMode::eExclusive,
	};
	vma::AllocationCreateInfo allocation_create_info{
		.flags = vma::AllocationCreateFlagBits::eHostAccessSequentialWrite | vma::AllocationCreateFlagBits::eMapped,
		.usage = vma::MemoryUsage::eAuto,
	};
	vma::AllocationInfo allocation_info;
	auto buffer_and_allocation = allocator.createBuffer(buffer_create_info, allocation_create_info, &allocation_info);
	return std::make_tuple(buffer_and_allocation.first, buffer_and_allocation.second, allocation_info.pMappedData);
}
