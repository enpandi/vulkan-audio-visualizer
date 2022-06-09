#include "Frame.hpp"

#include "constants.hpp"
#include <utility>

av::Frame::Frame(
	vk::raii::CommandBuffer &&command_buffer,
	GraphicsDevice const &gpu
)
	: _command_buffer{std::move(command_buffer)}
	, _draw_complete{gpu.device, {}}
	, _present_complete{gpu.device, {}}
	, _in_flight{gpu.device, {.flags = vk::FenceCreateFlagBits::eSignaled}} {}

av::Frames::Frames(
	size_t num_frames,
	const GraphicsDevice &gpu
) {
	this->reserve(num_frames);
	vk::CommandBufferAllocateInfo command_buffer_allocate_info{
		.commandPool = *gpu.graphics_command_pool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = av::constants::MAX_FRAMES_IN_FLIGHT,
	};
	for (auto &&command_buffer : vk::raii::CommandBuffers(gpu.device, command_buffer_allocate_info))
		this->emplace_back(std::move(command_buffer), gpu);
}
