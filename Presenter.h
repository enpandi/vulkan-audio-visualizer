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

		struct Vertex {
			struct Position {
				float x, y;
			} position;
			struct Color {
				float r, g, b;
			} color;
		private:
			friend class Presenter; // don't know if this is the best way to structure things
			static constexpr vk::VertexInputBindingDescription get_binding_description();
			static constexpr std::array<vk::VertexInputAttributeDescription, 2> get_attribute_descriptions();
		};

		Presenter(size_t const &num_vertices, bool const &floating, char const *title, size_t width = 320, size_t height = 240);

		~Presenter();

		// todo rule of 3? rule of 5? https://en.cppreference.com/w/cpp/language/rule_of_three

		[[nodiscard]] bool is_running() const;

		// upload vertices to the vertex buffer
		// todo this is probably not fast
		// todo try https://redd.it/aij7zp
		void set_vertices(Vertex const *vertices) const;

		void draw_frame();

	private:

		class QueueFamilyIndices {
		public:
			QueueFamilyIndices(vk::raii::PhysicalDevice const &physical_device, vk::raii::SurfaceKHR const &surface);

			[[nodiscard]] bool is_compatible() const;

			[[nodiscard]] uint32_t const &get_graphics_queue_family_index() const;

			[[nodiscard]] uint32_t const &get_present_queue_family_index() const;

		private:
			std::optional<uint32_t> graphics_queue_family_index;
			std::optional<uint32_t> present_queue_family_index;
		};

		class SwapchainInfo {
		public:
			// delegating constructor https://stackoverflow.com/a/61033668
			SwapchainInfo(vk::raii::PhysicalDevice const &physical_device, vk::raii::SurfaceKHR const &surface, vkfw::UniqueWindow const &window);

			[[nodiscard]] bool is_compatible() const;

			[[nodiscard]] uint32_t const &get_graphics_queue_family_index() const;

			[[nodiscard]] uint32_t const &get_present_queue_family_index() const;

			[[nodiscard]] vk::SurfaceCapabilitiesKHR const &get_surface_capabilities() const;

			[[nodiscard]] vk::SurfaceFormatKHR const &get_surface_format() const;

			[[nodiscard]] vk::PresentModeKHR const &get_present_mode() const;

			// Window vs UniqueWindow ?
			[[nodiscard]] vk::Extent2D get_extent() const;

		private:
			bool const supports_all_required_extensions;
			std::optional<uint32_t> const graphics_queue_family_index;
			std::optional<uint32_t> const present_queue_family_index;
			vk::SurfaceCapabilitiesKHR const surface_capabilities;
			std::optional<vk::SurfaceFormatKHR> const surface_format;
			std::optional<vk::PresentModeKHR> const present_mode;
			vkfw::UniqueWindow const &window;

			static bool check_supports_all_extensions(vk::raii::PhysicalDevice const &physical_device);

			static std::optional<vk::SurfaceFormatKHR>
			choose_surface_format(vk::raii::PhysicalDevice const &physical_device, vk::raii::SurfaceKHR const &surface);

			static std::optional<vk::PresentModeKHR> choose_present_mode(vk::raii::PhysicalDevice const &physical_device, vk::raii::SurfaceKHR const &surface);

			SwapchainInfo(
				bool const &supports_all_extensions,
				QueueFamilyIndices const &queue_family_indices,
				vk::SurfaceCapabilitiesKHR const &surface_capabilities,
				std::optional<vk::SurfaceFormatKHR> const &surface_format,
				std::optional<vk::PresentModeKHR> const &present_mode,
				vkfw::UniqueWindow const &window
			);
		};

		// todo read this https://www.khronos.org/blog/understanding-vulkan-synchronization
		// todo and this (vulkan 1.2+, not good for mobile) https://www.khronos.org/blog/vulkan-timeline-semaphores
		struct FrameSyncPrimitives {
			vk::raii::Semaphore draw_complete;
			vk::raii::Semaphore present_complete;
			vk::raii::Fence frame_in_flight;
			FrameSyncPrimitives(
				vk::raii::Device const &device,
				vk::SemaphoreCreateInfo const &image_available_semaphore_create_info,
				vk::SemaphoreCreateInfo const &render_finished_semaphore_create_info,
				vk::FenceCreateInfo const &in_flight_fence_create_info
			);
		};

		// are there constants for the layer/extension names?
		inline static std::vector<char const *> const GLOBAL_LAYERS{
#ifdef DEBUG
			"VK_LAYER_KHRONOS_validation"
#endif
		};
		static constexpr std::array DEVICE_EXTENSIONS = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
		inline static std::string const APPLICATION_NAME = "audio visualizer";
		inline static std::string const VERTEX_SHADER_FILE_NAME = "shaders/shader.vert.spv"; // todo can the strings be constexpr?
		inline static std::string const FRAGMENT_SHADER_FILE_NAME = "shaders/shader.frag.spv"; // https://stackoverflow.com/a/1563906
		static constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;

		size_t const num_vertices;

		// in case of ambiguity between vkfw and vk object identifiers, the vkfw identifier is prefixed with vkfw_
		// some are non-const for the sake of swapchain recreation
		vkfw::UniqueInstance const vkfw_instance;
		vkfw::UniqueWindow const window;
		vk::raii::Context const context;
		vk::raii::Instance const instance;
		vk::raii::SurfaceKHR const surface;
		vk::raii::PhysicalDevice const physical_device;
		SwapchainInfo swapchain_info;
		vk::raii::Device const device;
		vk::raii::SwapchainKHR swapchain;
		vk::raii::PipelineLayout const pipeline_layout;
		vk::raii::RenderPass render_pass;
		vk::raii::Pipeline pipeline;
		std::vector<vk::raii::ImageView> image_views;
		std::vector<vk::raii::Framebuffer> framebuffers;
		vk::raii::CommandPool const graphics_command_pool;
		vk::raii::CommandBuffers const graphics_command_buffers;
		vk::raii::Queue const graphics_queue;
		vk::raii::Queue const present_queue;
		std::vector<FrameSyncPrimitives> const frames_sync_primitives;
		vk::raii::Buffer const vertex_buffer;
		vk::raii::DeviceMemory const vertex_buffer_memory;

		uint32_t current_flight_frame = 0; // for multiple frames in flight
		bool framebuffer_resized = false;

		static vk::raii::Instance create_instance(vk::raii::Context const &);

		static vk::raii::PhysicalDevice choose_physical_device(vk::raii::Instance const &, vk::raii::SurfaceKHR const &);

		static vk::raii::Device create_device(vk::raii::PhysicalDevice const &, av::Presenter::SwapchainInfo const &);

		static vk::raii::SwapchainKHR create_swapchain(
			vk::raii::Device const &,
			vk::raii::SurfaceKHR const &,
			av::Presenter::SwapchainInfo const &,
			vk::raii::SwapchainKHR const &old_swapchain
		);

		static vk::raii::PipelineLayout create_pipeline_layout(vk::raii::Device const &);

		static vk::raii::RenderPass create_render_pass(vk::raii::Device const &, av::Presenter::SwapchainInfo const &);

		static vk::raii::Pipeline create_pipeline(
			vk::raii::Device const &,
			av::Presenter::SwapchainInfo const &,
			vk::raii::PipelineLayout const &,
			vk::raii::RenderPass const &
		);

		static std::vector<vk::raii::ImageView> create_image_views(
			vk::raii::Device const &,
			vk::raii::SwapchainKHR const &,
			av::Presenter::SwapchainInfo const &);

		static std::vector<vk::raii::Framebuffer> create_framebuffers(
			vk::raii::Device const &,
			vk::raii::RenderPass const &,
			std::vector<vk::raii::ImageView> const &,
			av::Presenter::SwapchainInfo const &
		);

		static vk::raii::CommandPool create_command_pool(vk::raii::Device const &, av::Presenter::SwapchainInfo const &);

		static vk::raii::CommandBuffers allocate_command_buffers(vk::raii::Device const &, vk::raii::CommandPool const &graphics_command_pool);

		static std::vector<FrameSyncPrimitives> create_frame_sync_signalers(vk::raii::Device const &);

		static vk::raii::ShaderModule create_shader_module(vk::raii::Device const &, std::vector<char> const &code_chars);

		static vk::raii::Buffer create_vertex_buffer(vk::raii::Device const &, size_t const &num_vertices);

		static vk::raii::DeviceMemory allocate_vertex_buffer_memory(
			vk::raii::Device const &,
			vk::raii::Buffer const &vertex_buffer,
			vk::raii::PhysicalDevice const &
		);


		void bind_vertex_buffer_memory() const;

		void recreate_swapchain();

		void record_graphics_command_buffer(vk::raii::CommandBuffer const &command_buffer, vk::raii::Framebuffer const &framebuffer) const;

		static std::vector<char> file_to_chars(std::string const &file_name);
	};
}

#endif //AUDIO_VISUALIZER_PRESENTER_H
