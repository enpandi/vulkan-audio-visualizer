#ifndef AUDIO_VISUALIZER_GRAPHICSDEVICE_HPP
#define AUDIO_VISUALIZER_GRAPHICSDEVICE_HPP

#include "graphics_headers.hpp"
#include <optional>

namespace av {
	struct GraphicsDevice {
		GraphicsDevice(vk::raii::Instance const &, vk::raii::SurfaceKHR const &);

		struct QueueFamilyIndices {
			uint32_t const graphics;
			uint32_t const present;
			static std::optional<QueueFamilyIndices> get_queue_family_indices(
				vk::raii::PhysicalDevice const &,
				vk::raii::SurfaceKHR const &
			);
		};

		vk::raii::PhysicalDevice const physical_device;
		QueueFamilyIndices const queue_family_indices;
		vk::raii::Device const device;
		vk::raii::Queue const graphics_queue;
		vk::raii::Queue const present_queue;
		vk::raii::CommandPool const graphics_command_pool;

	private:
		static bool physical_device_is_compatible(
			vk::raii::PhysicalDevice const &,
			vk::raii::SurfaceKHR const &
		);

		static vk::raii::PhysicalDevice choose_physical_device(
			vk::raii::Instance const &,
			vk::raii::SurfaceKHR const &
		);

		static vk::raii::Device create_device(
			vk::raii::PhysicalDevice const &,
			QueueFamilyIndices const &
		);

		static vk::raii::CommandPool create_graphics_command_pool(
			vk::raii::Device const &,
			uint32_t graphics_queue_family_index
		);
	};
} // av

#endif //AUDIO_VISUALIZER_GRAPHICSDEVICE_HPP
