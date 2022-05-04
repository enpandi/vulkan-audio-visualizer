//
// Created by panda on 22/05/03.
//

#ifndef AUDIO_VISUALIZER_PRESENTER_H
#define AUDIO_VISUALIZER_PRESENTER_H

#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_NO_SETTERS
#define VKFW_NO_STRUCT_CONSTRUCTORS
#include <vkfw/vkfw.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <optional>

namespace av {
	class Presenter {
	public:
		Presenter();

		~Presenter();

		[[nodiscard]] bool running() const;

		void draw_frame();

//		void loop() const;

	private:

		class SwapchainInfo {
		public:
			// delegating constructor https://stackoverflow.com/a/61033668
			SwapchainInfo(vk::raii::PhysicalDevice const &physical_device, vk::raii::SurfaceKHR const &surface);

			[[nodiscard]] bool is_compatible() const;

			[[nodiscard]] uint32_t const &get_graphics_queue_family_index() const;

			[[nodiscard]] uint32_t const &get_present_queue_family_index() const;

			[[nodiscard]] vk::SurfaceCapabilitiesKHR const &get_surface_capabilities() const;

			[[nodiscard]] vk::SurfaceFormatKHR const &get_surface_format() const;

			[[nodiscard]] vk::PresentModeKHR const &get_present_mode() const;

			// Window vs UniqueWindow ?
			[[nodiscard]] vk::Extent2D get_extent(vkfw::UniqueWindow const &window) const;

		private:
			bool const supports_all_required_extensions;
			std::optional<uint32_t> const graphics_queue_family_index;
			std::optional<uint32_t> const present_queue_family_index;
			vk::SurfaceCapabilitiesKHR const surface_capabilities;
			std::optional<vk::SurfaceFormatKHR> const surface_format;
			std::optional<vk::PresentModeKHR> const present_mode;

			static bool check_supports_all_extensions(vk::raii::PhysicalDevice const &physical_device);

			struct QueueFamilyIndices {
				std::optional<uint32_t> graphics_queue_family_index;
				std::optional<uint32_t> present_queue_family_index;

				QueueFamilyIndices(vk::raii::PhysicalDevice const &physical_device, vk::raii::SurfaceKHR const &surface);
			};

			static std::optional<vk::SurfaceFormatKHR>
			choose_surface_format(vk::raii::PhysicalDevice const &physical_device, vk::raii::SurfaceKHR const &surface);

			static std::optional<vk::PresentModeKHR> choose_present_mode(vk::raii::PhysicalDevice const &physical_device, vk::raii::SurfaceKHR const &surface);

			SwapchainInfo(
				bool const &supports_all_extensions,
				QueueFamilyIndices const &queue_family_indices,
				vk::SurfaceCapabilitiesKHR const &surface_capabilities,
				std::optional<vk::SurfaceFormatKHR> const &surface_format,
				std::optional<vk::PresentModeKHR> const &present_mode
			);
		};

		// todo read this https://www.khronos.org/blog/understanding-vulkan-synchronization
		// todo and this (vulkan 1.2+, not good for mobile) https://www.khronos.org/blog/vulkan-timeline-semaphores
		struct FrameSyncSignaler {
			vk::raii::Semaphore image_available_semaphore;
			vk::raii::Semaphore render_finished_semaphore;
			vk::raii::Fence in_flight_fence;
			FrameSyncSignaler(
				vk::raii::Device const &device,
				vk::SemaphoreCreateInfo const &image_available_semaphore_create_info,
				vk::SemaphoreCreateInfo const &render_finished_semaphore_create_info,
				vk::FenceCreateInfo const &in_flight_fence_create_info
			);
		};

		static std::array constexpr GLOBAL_LAYERS = {"VK_LAYER_KHRONOS_validation"}; // todo is there a macro for this, or:
		static std::array constexpr DEVICE_EXTENSIONS = {VK_KHR_SWAPCHAIN_EXTENSION_NAME}; // todo is there a vulkan-hpp constant for this
		inline static std::string const APPLICATION_NAME = "audio visualizer";
		inline static std::string const VERTEX_SHADER_FILE_NAME = "shaders/shader.vert.spv"; // todo can the strings be constexpr?
		inline static std::string const FRAGMENT_SHADER_FILE_NAME = "shaders/shader.frag.spv"; // https://stackoverflow.com/a/1563906
		static constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;

		// vkfw things are prefixed vkfw_
		// vk things are not prefixed
		vkfw::UniqueInstance const vkfw_instance;
		vkfw::UniqueWindow const vkfw_window;
		vk::raii::Context const context;
		vk::raii::Instance const instance;
		vk::raii::SurfaceKHR const surface;
		vk::raii::PhysicalDevice const physical_device;
		SwapchainInfo swapchain_info; // non-const for swapchain recreation
		vk::raii::Device const device;
		vk::raii::PipelineLayout const pipeline_layout;
		vk::raii::Queue const graphics_queue; // queue indices depend on swapchain info but should not change upon VK_ERROR_OUT_OF_DATE_KHR
		vk::raii::CommandPool const graphics_command_pool;
		vk::raii::CommandBuffers const graphics_command_buffers;
		vk::raii::Queue const present_queue; // at least i think they shouldn't change
		std::vector<FrameSyncSignaler> const frame_sync_signalers;

		// swapchain objects that might be recreated:
		vk::Format swapchain_format;
		vk::Extent2D swapchain_extent;
		vk::raii::SwapchainKHR swapchain;
		vk::raii::RenderPass render_pass;
		vk::raii::Pipeline pipeline;
		std::vector<vk::raii::ImageView> image_views;
		std::vector<vk::raii::Framebuffer> framebuffers;

		uint32_t current_flight_frame; // for multiple frames in flight

		[[nodiscard]] vk::raii::Instance create_instance() const;

		[[nodiscard]] vk::raii::PhysicalDevice choose_physical_device() const;

		[[nodiscard]] vk::raii::Device create_device() const;

		[[nodiscard]] vk::raii::PipelineLayout create_pipeline_layout() const;

		[[nodiscard]] vk::raii::CommandPool create_command_pool() const;

		[[nodiscard]] vk::raii::CommandBuffers allocate_command_buffers() const;

		[[nodiscard]] std::vector<FrameSyncSignaler> create_frame_sync_signalers() const;


		[[nodiscard]] vk::raii::SwapchainKHR create_swapchain() const;

		[[nodiscard]] vk::raii::RenderPass create_render_pass() const;

		[[nodiscard]] vk::raii::Pipeline create_pipeline() const;

		[[nodiscard]] std::vector<vk::raii::ImageView> create_image_views() const;

		[[nodiscard]] std::vector<vk::raii::Framebuffer> create_framebuffers() const;


		[[nodiscard]] vk::raii::ShaderModule create_shader_module(std::vector<char> const &code_chars) const;

		void record_graphics_command_buffer(vk::raii::CommandBuffer const &command_buffer, vk::raii::Framebuffer const &framebuffer) const;

		static std::vector<char> file_to_chars(std::string const &file_name);
	};
}


#endif //AUDIO_VISUALIZER_PRESENTER_H
