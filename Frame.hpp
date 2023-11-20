#ifndef AUDIO_VISUALIZER_FRAME_HPP
#define AUDIO_VISUALIZER_FRAME_HPP

#include "Gpu.hpp"
#include "graphics_headers.hpp"
#include <vector>

namespace av {
	class Frame {
	public:
		Frame(
			vk::raii::CommandBuffer &&,
			Gpu const &
		);
		vk::raii::CommandBuffer const &command_buffer{_command_buffer};
		vk::raii::Semaphore const &draw_complete{_draw_complete};
		vk::raii::Semaphore const &present_complete{_present_complete};
		vk::raii::Fence const &in_flight{_in_flight};
	private:
		// todo read this https://www.khronos.org/blog/understanding-vulkan-synchronization
		// todo and this (vulkan 1.2+, not good for mobile) https://www.khronos.org/blog/vulkan-timeline-semaphores
		vk::raii::CommandBuffer _command_buffer;
		vk::raii::Semaphore _draw_complete;
		vk::raii::Semaphore _present_complete;
		vk::raii::Fence _in_flight;
	};

	class Frames : std::vector<Frame> {
	public:
		Frames(size_t num_frames, GraphicsDevice const &);
		using std::vector<Frame>::operator[];
	};
} // av

#endif //AUDIO_VISUALIZER_FRAME_HPP
