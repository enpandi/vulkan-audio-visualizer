#include "Frame.hpp"

#include "constants.hpp"
#include <utility>

namespace av {
	Frame::Frame(
		vk::raii::CommandBuffer &&command_buffer,
		GraphicsDevice const &gpu
	)
		: _command_buffer{std::move(command_buffer)}
		, _draw_complete{gpu.device, vk::SemaphoreCreateInfo{}}
		, _present_complete{gpu.device, vk::SemaphoreCreateInfo{}}
		, _in_flight{gpu.device, {.flags = vk::FenceCreateFlagBits::eSignaled}} {}

	Frames::Frames(
		size_t num_frames,
		const GraphicsDevice &gpu
	) {
		reserve(num_frames);
		vk::CommandBufferAllocateInfo command_buffer_allocate_info{
			.commandPool = *gpu.graphics_command_pool,
			.level = vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = num_frames,
		};
		for (auto &&command_buffer : vk::raii::CommandBuffers(gpu.device, command_buffer_allocate_info))
			emplace_back(std::move(command_buffer), gpu);
	}
} // av
