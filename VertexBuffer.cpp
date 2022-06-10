#include "VertexBuffer.hpp"

#include <algorithm>
#include <tuple>
#include <vector>

#if 0
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
	std::copy(vertices.begin(), vertices.end(), _mapped_data);
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
	, _mapped_data{static_cast<Vertex *const>(std::get<void *>(buffer_objects))} {}

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
#endif

vk::raii::Buffer create_vertex_buffer(vk::raii::Device const &device, size_t const &num_vertices) {
	vk::BufferCreateInfo buffer_create_info{
		.size = num_vertices * sizeof(av::Vertex),
		.usage = vk::BufferUsageFlagBits::eVertexBuffer,
		.sharingMode = vk::SharingMode::eExclusive,
	};
	return {device, buffer_create_info};
};

std::optional<uint32_t> get_memory_type_index(
	vk::raii::PhysicalDevice const &physical_device,
	uint32_t memory_type_bits,
	vk::MemoryPropertyFlags memory_property_flags
) {
	vk::PhysicalDeviceMemoryProperties memory_properties = physical_device.getMemoryProperties();
	for (uint32_t memory_type_index = 0; memory_type_index < memory_properties.memoryTypeCount; ++memory_type_index)
		if ((memory_type_bits & (1 << memory_type_index))
		    && !(~memory_properties.memoryTypes[memory_type_index].propertyFlags & memory_property_flags))
			return memory_type_index;
	return std::nullopt;
}

vk::raii::DeviceMemory allocate_vertex_buffer_memory(
	vk::raii::Device const &device,
	vk::raii::Buffer const &vertex_buffer,
	vk::raii::PhysicalDevice const &physical_device
) {
	vk::MemoryRequirements memory_requirements = vertex_buffer.getMemoryRequirements();
	uint32_t memory_type_index;
	{
		std::optional<uint32_t> optional_memory_type_index = get_memory_type_index(
			physical_device,
			memory_requirements.memoryTypeBits,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
		);
		if (!optional_memory_type_index.has_value())
			throw std::runtime_error("failed to find suitable memory type");
		memory_type_index = *optional_memory_type_index;
	}
	vk::MemoryAllocateInfo memory_allocate_info{
		.allocationSize = memory_requirements.size,
		.memoryTypeIndex = memory_type_index,
	};
	return {device, memory_allocate_info};
}

av::VertexBuffer::VertexBuffer(size_t num_vertices, av::GraphicsDevice const &gpu)
	: num_vertices{num_vertices}
	, buffer{create_vertex_buffer(gpu.device, num_vertices)}
	, memory{allocate_vertex_buffer_memory(
		gpu.device, buffer, gpu.physical_device)}
	, _mapped_data{static_cast<Vertex *const>(memory.mapMemory(0, num_vertices * sizeof(Vertex), {}))} {
	buffer.bindMemory(*memory, 0);
}

void av::VertexBuffer::set_vertices(std::vector<Vertex> const &vertices) {
	num_vertices = vertices.size();
	std::copy(vertices.begin(), vertices.end(), _mapped_data);
}

void av::VertexBuffer::bind_and_draw(vk::raii::CommandBuffer const &command_buffer) const {
	command_buffer.bindVertexBuffers(0, {*buffer}, {0});
	command_buffer.draw(num_vertices, 1, 0, 0);
}
