#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iomanip>
#include <ios>
#include <iostream>
#include <limits>
#include <ranges>
#include <utility>

#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_NO_SETTERS
#define VKFW_NO_STRUCT_CONSTRUCTORS
#include <vkfw/vkfw.hpp>
#include <vulkan/vulkan_raii.hpp>

// todo disable stuff in release mode, for now assume debug mode always

static std::array const vk_global_layers = {"VK_LAYER_KHRONOS_validation"}; // todo is there a macro for this, or:
static std::array const vk_device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME}; // todo is there a vulkan-hpp constant for this
static std::string const application_name = "audio visualizer";
static std::string const vertex_shader_file_name = "shaders/shader.vert.spv";
static std::string const fragment_shader_file_name = "shaders/shader.frag.spv";

// perf doesn't work on windows
namespace timer {
	std::chrono::time_point<std::chrono::steady_clock> start_time;

	void start() {
		start_time = std::chrono::steady_clock::now();
	}

	void stop(char const *message) {
		auto stop_time = std::chrono::steady_clock::now();
		auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(stop_time - start_time).count();
		std::cout << std::right << std::setw(15) << static_cast<long double>(nanos) / 1e9 << " s - " << message << std::endl;
	}
}

vk::raii::Instance vk_create_instance(vk::raii::Context const &context) {
	vk::ApplicationInfo application_info{
		.pApplicationName = application_name.c_str(),
		.apiVersion = VK_API_VERSION_1_1,
	};
	std::span<char const *const> glfw_extensions = vkfw::getRequiredInstanceExtensions();
	vk::InstanceCreateInfo instance_create_info{
		.pApplicationInfo = &application_info,
		.enabledLayerCount = vk_global_layers.size(),
		.ppEnabledLayerNames = vk_global_layers.data(),
		.enabledExtensionCount = static_cast<uint32_t>(glfw_extensions.size()),
		.ppEnabledExtensionNames = glfw_extensions.data(),
	};
	return context.createInstance(instance_create_info);
}

class SwapchainInfo {
	bool const supports_all_extensions;
	std::optional<std::uint32_t> const graphics_queue_family_index;
	std::optional<std::uint32_t> const present_queue_family_index;
	vk::SurfaceCapabilitiesKHR const surface_capabilities;
	std::optional<vk::SurfaceFormatKHR> const surface_format;
	std::optional<vk::PresentModeKHR> const present_mode;

public:
	// delegating constructor https://stackoverflow.com/a/61033668
	SwapchainInfo(
		vk::raii::PhysicalDevice const &physical_device,
		vk::raii::SurfaceKHR const &surface
	) : SwapchainInfo(
		check_supports_all_extensions(physical_device),
		QueueFamilyIndices(physical_device, surface),
		physical_device.getSurfaceCapabilitiesKHR(*surface),
		choose_surface_format(physical_device, surface),
		choose_present_mode(physical_device, surface)
	) {}

	[[nodiscard]] bool is_compatible() const {
		return supports_all_extensions
		       && graphics_queue_family_index.has_value()
		       && present_queue_family_index.has_value()
		       && surface_format.has_value()
		       && present_mode.has_value();
	}

	// prefer value() over operator* because std::bad_optional_access seems like a useful thing to throw

	[[nodiscard]] std::uint32_t const &get_graphics_queue_family_index() const { return graphics_queue_family_index.value(); }

	[[nodiscard]] std::uint32_t const &get_present_queue_family_index() const { return present_queue_family_index.value(); }

	[[nodiscard]] vk::SurfaceCapabilitiesKHR const &get_surface_capabilities() const { return surface_capabilities; }

	[[nodiscard]] vk::SurfaceFormatKHR const &get_surface_format() const { return surface_format.value(); }

	[[nodiscard]] vk::PresentModeKHR const &get_present_mode() const { return present_mode.value(); }

	// Window vs UniqueWindow ?
	vk::Extent2D get_extent(vkfw::UniqueWindow const &window) const {
		if (surface_capabilities.currentExtent == vk::Extent2D{
			.width = 0xFFFFFFFF,
			.height = 0xFFFFFFFF,
			// https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VkSurfaceCapabilitiesKHR.html
			// currentExtent is the current width and height of the surface, or the special value (0xFFFFFFFF, 0xFFFFFFFF)
			// indicating that the surface size will be determined by the extent of a swapchain targeting the surface.
		})
			return surface_capabilities.currentExtent;
		auto[width, height] = window->getFramebufferSize();
		return {
			.width = std::clamp<uint32_t>(width, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width),
			.height = std::clamp<uint32_t>(height, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height),
		};
	}

private:
	static bool check_supports_all_extensions(vk::raii::PhysicalDevice const &physical_device) {
		std::vector<vk::ExtensionProperties> extensions_properties = physical_device.enumerateDeviceExtensionProperties();
		return std::ranges::all_of(
			vk_device_extensions,
			[&](std::string const &device_extension) {
				return std::ranges::any_of(
					extensions_properties,
					[&](vk::ExtensionProperties const &extension_properties) {
						return device_extension == extension_properties.extensionName;
					});
//					extensions_properties | std::views::transform(&vk::ExtensionProperties::extensionName),
//					std::bind(std::equal_to<std::string>(), device_extension, std::placeholders::_1));
			});
	}

	struct QueueFamilyIndices {
		std::optional<std::uint32_t> graphics_queue_family_index;
		std::optional<std::uint32_t> present_queue_family_index;

		QueueFamilyIndices(vk::raii::PhysicalDevice const &physical_device, vk::raii::SurfaceKHR const &surface) {
			std::vector<vk::QueueFamilyProperties> queue_families_properties = physical_device.getQueueFamilyProperties();
			for (std::uint32_t queue_family_index = 0; queue_family_index < queue_families_properties.size(); ++queue_family_index) {
				bool supports_graphics = static_cast<bool>(queue_families_properties[queue_family_index].queueFlags & vk::QueueFlagBits::eGraphics);
				bool supports_present = physical_device.getSurfaceSupportKHR(queue_family_index, *surface);
				if (supports_graphics && supports_present) {
					graphics_queue_family_index = present_queue_family_index = queue_family_index;
					break;
				}
				if (supports_graphics && !graphics_queue_family_index.has_value())
					graphics_queue_family_index = queue_family_index;
				if (supports_present && !present_queue_family_index.has_value())
					present_queue_family_index = queue_family_index;
			}
		}
	};

	static std::optional<vk::SurfaceFormatKHR> choose_surface_format(vk::raii::PhysicalDevice const &physical_device, vk::raii::SurfaceKHR const &surface) {
		std::vector<vk::SurfaceFormatKHR> surface_formats = physical_device.getSurfaceFormatsKHR(*surface);
		if (surface_formats.empty())
			return std::nullopt;
		for (vk::SurfaceFormatKHR const &surface_format: surface_formats)
			if (surface_format.format == vk::Format::eB8G8R8A8Srgb
			    && surface_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
				return surface_format;
		return surface_formats.front();
		// todo mess around with this
	}

	static std::optional<vk::PresentModeKHR> choose_present_mode(vk::raii::PhysicalDevice const &physical_device, vk::raii::SurfaceKHR const &surface) {
		std::vector<vk::PresentModeKHR> present_modes = physical_device.getSurfacePresentModesKHR(*surface);
		if (present_modes.empty())
			return std::nullopt;
		for (vk::PresentModeKHR const &present_mode: present_modes)
			if (present_mode == vk::PresentModeKHR::eMailbox)
				return present_mode;
		return vk::PresentModeKHR::eFifo;
		// todo mess around with this e.g. immediate, fifo relaxed
		// todo prefer fifo for low energy consumption
	}

	SwapchainInfo(
		bool const &supports_all_extensions,
		QueueFamilyIndices const &queue_family_indices,
		vk::SurfaceCapabilitiesKHR const &surface_capabilities,
		std::optional<vk::SurfaceFormatKHR> const &surface_format,
		std::optional<vk::PresentModeKHR> const &present_mode
	) : supports_all_extensions{supports_all_extensions},
	    graphics_queue_family_index{queue_family_indices.graphics_queue_family_index},
	    present_queue_family_index{queue_family_indices.present_queue_family_index},
	    surface_capabilities{surface_capabilities},
	    surface_format{surface_format},
	    present_mode{present_mode} {}
};

std::pair<vk::raii::PhysicalDevice, SwapchainInfo> vk_choose_physical_device(vk::raii::Instance const &instance, vk::raii::SurfaceKHR const &surface) {
	vk::raii::PhysicalDevices physical_devices(instance);
	std::vector<SwapchainInfo> swapchain_infos;
	swapchain_infos.reserve(physical_devices.size());
	std::transform(physical_devices.begin(), physical_devices.end(), std::back_inserter(swapchain_infos),
	               [&](vk::raii::PhysicalDevice const &physical_device) {
		               return SwapchainInfo(physical_device, surface);
	               });
	auto usable_physical_device_indices = std::views::iota(0u, physical_devices.size())
	                                      | std::views::filter(
		[&](std::uint32_t const &physical_device_index) {
			return swapchain_infos[physical_device_index].is_compatible();
		});
	if (!usable_physical_device_indices)
		throw std::runtime_error("vulkan failed to find usable physical devices.");
	for (std::uint32_t physical_device_index: usable_physical_device_indices)
		if (physical_devices[physical_device_index].getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
			return {std::move(physical_devices[physical_device_index]), swapchain_infos[physical_device_index]};
	return {std::move(physical_devices.front()), swapchain_infos.front()};
}

vk::raii::Device vk_create_device(vk::raii::PhysicalDevice const &physical_device) {
	std::vector<vk::QueueFamilyProperties> queue_families_properties = physical_device.getQueueFamilyProperties();
	uint32_t graphics_queue_family_index = std::distance(
		queue_families_properties.begin(),
		std::ranges::find_if(
			queue_families_properties,
			[](vk::QueueFamilyProperties const &queue_family_properties) {
				return static_cast<bool>(queue_family_properties.queueFlags & vk::QueueFlagBits::eGraphics);
			}));
	float queue_priority = 0.0f;
	vk::DeviceQueueCreateInfo device_queue_create_info{
		.queueFamilyIndex = graphics_queue_family_index,
		.queueCount = 1,
		.pQueuePriorities = &queue_priority,
	};
	vk::PhysicalDeviceFeatures enabled_features;
	vk::DeviceCreateInfo device_create_info{
		.queueCreateInfoCount = 1,
		.pQueueCreateInfos = &device_queue_create_info,
		.enabledLayerCount = vk_global_layers.size(),
		.ppEnabledLayerNames = vk_global_layers.data(),
		.enabledExtensionCount = vk_device_extensions.size(),
		.ppEnabledExtensionNames = vk_device_extensions.data(),
		.pEnabledFeatures = &enabled_features,
	};
	return physical_device.createDevice(device_create_info);
}

vk::raii::SwapchainKHR vk_create_swapchain(
	vk::raii::Device const &device,
	vk::raii::SurfaceKHR const &surface,
	SwapchainInfo const &swapchain_info,
	vk::Extent2D const &extent
) {
	std::uint32_t const &graphics_queue_family_index = swapchain_info.get_graphics_queue_family_index();
	std::uint32_t const &present_queue_family_index = swapchain_info.get_present_queue_family_index();
	std::array queue_family_index_array = {graphics_queue_family_index, present_queue_family_index};
	vk::SurfaceCapabilitiesKHR const &surface_capabilities = swapchain_info.get_surface_capabilities();
	vk::SwapchainCreateInfoKHR swapchain_create_info{
		.surface = *surface,
		.minImageCount = surface_capabilities.maxImageCount == 0
		                 ? surface_capabilities.minImageCount + 1
		                 : std::min(surface_capabilities.minImageCount + 1, surface_capabilities.maxImageCount),
		.imageFormat = swapchain_info.get_surface_format().format,
		.imageColorSpace = swapchain_info.get_surface_format().colorSpace,
		.imageExtent = extent,
		.imageArrayLayers = 1,
		.imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
		.imageSharingMode = graphics_queue_family_index == present_queue_family_index ?
		                    vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent,
		.queueFamilyIndexCount = queue_family_index_array.size(),
		.pQueueFamilyIndices = queue_family_index_array.data(),
		.preTransform = surface_capabilities.currentTransform, // default???
		.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque, // default
		.presentMode = swapchain_info.get_present_mode(),
		.clipped = true,
//		.oldSwapchain = VK_NULL_HANDLE, // ???
	};
	return device.createSwapchainKHR(swapchain_create_info);
}

std::vector<vk::raii::ImageView> vk_create_image_views(std::vector<VkImage> const &images, vk::raii::Device const &device, vk::Format const &format) {
	std::vector<vk::raii::ImageView> image_views;
	image_views.reserve(images.size());
	for (VkImage const &image: images) {
		vk::ImageViewCreateInfo image_view_create_info{
			.image = image,
			.viewType = vk::ImageViewType::e2D,
			.format = format,
			.components{
				.r = vk::ComponentSwizzle::eIdentity,
				.g = vk::ComponentSwizzle::eIdentity,
				.b = vk::ComponentSwizzle::eIdentity,
				.a = vk::ComponentSwizzle::eIdentity,
			}, // todo mess with the color swizzling here
			.subresourceRange{
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		};
		image_views.emplace_back(device, image_view_create_info);
	}
	return image_views;
}

std::vector<char> file_to_chars(std::string const &file_name) {
	std::ifstream file(file_name, std::ios::binary | std::ios::ate);
	if (!file.is_open())
		throw std::ios::failure("failed to open file");
	std::vector<char> chars(file.tellg());
	file.seekg(0);
	file.read(chars.data(), static_cast<std::streamsize>(chars.size()));
	file.close();
	return chars;
}

vk::raii::PipelineLayout vk_create_pipeline_layout(vk::raii::Device const &device) {
	vk::PipelineLayoutCreateInfo pipeline_layout_create_info{
		.setLayoutCount = 0,
		.pushConstantRangeCount = 0,
	};
	return device.createPipelineLayout(pipeline_layout_create_info);
}

vk::raii::RenderPass vk_create_render_pass(vk::raii::Device const &device, vk::Format const &format) {
	vk::AttachmentDescription attachment_description{
		.format = format,
		.samples = vk::SampleCountFlagBits::e1,
		.loadOp = vk::AttachmentLoadOp::eClear,  // todo experiment with these, particularly dontcare
		.storeOp = vk::AttachmentStoreOp::eStore,
		.stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
		.stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
		.initialLayout = vk::ImageLayout::eUndefined,
		.finalLayout = vk::ImageLayout::ePresentSrcKHR,
	};
	vk::AttachmentReference attachment_reference{
		.attachment = 0,
		.layout = vk::ImageLayout::eColorAttachmentOptimal,
	};
	vk::SubpassDescription subpass_description{
		.pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
		.colorAttachmentCount = 1,
		.pColorAttachments = &attachment_reference,
	};
	vk::SubpassDependency subpass_dependency{
		.srcSubpass = VK_SUBPASS_EXTERNAL, // todo find a macro for this
		.dstSubpass = 0,
		.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
		.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
		.srcAccessMask = {},
		.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
//		.dependencyFlags = ,
	}; // this forms a dependency graph
	// todo i dont understand subpasses
	vk::RenderPassCreateInfo render_pass_create_info{
		.attachmentCount = 1,
		.pAttachments = &attachment_description,
		.subpassCount = 1,
		.pSubpasses = &subpass_description,
		.dependencyCount = 1,
		.pDependencies = &subpass_dependency,
	};
	return device.createRenderPass(render_pass_create_info);
}

vk::raii::ShaderModule vk_create_shader_module(vk::raii::Device const &device, std::vector<char> const &code_chars) {
	vk::ShaderModuleCreateInfo shader_module_create_info{
		.codeSize = code_chars.size(),
		.pCode = reinterpret_cast<uint32_t const *>(code_chars.data()),
	};
	return device.createShaderModule(shader_module_create_info);
}

vk::raii::Pipeline vk_create_pipeline(
	vk::raii::Device const &device, vk::Extent2D const &extent,
	vk::raii::PipelineLayout const &pipeline_layout, vk::raii::RenderPass const &render_pass
) {
	std::vector<char> vertex_shader_code = file_to_chars(vertex_shader_file_name);
	vk::raii::ShaderModule vertex_shader_module = vk_create_shader_module(device, vertex_shader_code);
	vk::PipelineShaderStageCreateInfo vertex_shader_stage_create_info{
		.stage = vk::ShaderStageFlagBits::eVertex, // why is this default
		.module = *vertex_shader_module,
		.pName = "main",
	};
	std::vector<char> fragment_shader_code = file_to_chars(fragment_shader_file_name);
	vk::raii::ShaderModule fragment_shader_module = vk_create_shader_module(device, fragment_shader_code);
	vk::PipelineShaderStageCreateInfo fragment_shader_stage_info{
		.stage = vk::ShaderStageFlagBits::eFragment,
		.module = *fragment_shader_module,
		.pName = "main",
	};
	std::array pipeline_shader_stage_create_infos{vertex_shader_stage_create_info, fragment_shader_stage_info};
	vk::PipelineVertexInputStateCreateInfo pipeline_vertex_input_state_create_info{
		.vertexBindingDescriptionCount = 0,
		.vertexAttributeDescriptionCount = 0,
	}; // todo
	vk::PipelineInputAssemblyStateCreateInfo pipeline_input_assembly_state_create_info{
		.topology = vk::PrimitiveTopology::eTriangleList,
		.primitiveRestartEnable = false,
	};
//	vk::PipelineTessellationStateCreateInfo tesselation_state_info{};
	vk::Viewport viewport{
		.x = 0.0f,
		.y = 0.0f,
		.width = static_cast<float>(extent.width),
		.height = static_cast<float>(extent.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};
	vk::Rect2D scissor{
		.offset{.x=0, .y=0},
		.extent = extent,
	};
	vk::PipelineViewportStateCreateInfo pipeline_viewport_state_create_info{
		.viewportCount = 1,
		.pViewports = &viewport,
		.scissorCount = 1,
		.pScissors = &scissor,
	};
	vk::PipelineRasterizationStateCreateInfo pipeline_rasterization_state_create_info{
		.depthClampEnable = false,
		.rasterizerDiscardEnable = false, // todo what is this
		.polygonMode = vk::PolygonMode::eFill, // todo mess with these settings
		.cullMode = vk::CullModeFlagBits::eBack,
		.frontFace = vk::FrontFace::eClockwise,
		.depthBiasEnable = false,
		.lineWidth = 1.0f,
	};
	vk::PipelineMultisampleStateCreateInfo pipeline_multisample_state_create_info{
		.rasterizationSamples = vk::SampleCountFlagBits::e1, // default
		.sampleShadingEnable = false, // hmm
	};
	vk::PipelineColorBlendAttachmentState pipeline_color_blend_attachment_state{
		.blendEnable = false, // todo what if it's true? UHHHHH
		.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
	};
	vk::PipelineColorBlendStateCreateInfo pipeline_color_blend_state_create_info{
		.logicOpEnable = false,
//		.logicOp = vk::LogicOp::eCopy, // the default is clear?
		.attachmentCount = 1,
		.pAttachments = &pipeline_color_blend_attachment_state,
//		.blendConstants = {}, // what is this
	};


	vk::GraphicsPipelineCreateInfo graphics_pipeline_create_info{
		.stageCount = pipeline_shader_stage_create_infos.size(),
		.pStages = pipeline_shader_stage_create_infos.data(),
		.pVertexInputState = &pipeline_vertex_input_state_create_info,
		.pInputAssemblyState = &pipeline_input_assembly_state_create_info,
//		.pTessellationState,
		.pViewportState = &pipeline_viewport_state_create_info,
		.pRasterizationState = &pipeline_rasterization_state_create_info,
		.pMultisampleState = &pipeline_multisample_state_create_info,
//		.pDepthStencilState,
		.pColorBlendState = &pipeline_color_blend_state_create_info,
//		.pDynamicState,
		.layout = *pipeline_layout,
		.renderPass = *render_pass,
		.subpass = 0,
//		.basePipelineHandle,
//		.basePipelineIndex,
	};
	return device.createGraphicsPipeline(nullptr, graphics_pipeline_create_info);
}

std::vector<vk::raii::Framebuffer>
vk_create_framebuffers(
	vk::raii::Device const &device, std::vector<vk::raii::ImageView> const &image_views,
	vk::raii::RenderPass const &render_pass, vk::Extent2D const &extent
) {
	std::vector<vk::raii::Framebuffer> framebuffers;
	framebuffers.reserve(image_views.size());
	for (vk::raii::ImageView const &image_view: image_views) {
		std::array attachments{*image_view};
		vk::FramebufferCreateInfo framebuffer_create_info{
			.renderPass = *render_pass,
			.attachmentCount = attachments.size(),
			.pAttachments = attachments.data(),
			.width = extent.width,
			.height = extent.height,
			.layers = 1,
		};
		framebuffers.emplace_back(device, framebuffer_create_info);
	}
	return framebuffers;
}
// todo rename extent

vk::raii::CommandPool vk_create_command_pool(vk::raii::Device const &device, uint32_t graphics_queue_family_index) {
	vk::CommandPoolCreateInfo command_pool_create_info{
		.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		.queueFamilyIndex = graphics_queue_family_index,
	};
	return device.createCommandPool(command_pool_create_info);
}

vk::raii::CommandBuffer vk_allocate_command_buffer(vk::raii::Device const &device, vk::raii::CommandPool const &command_pool) {
	vk::CommandBufferAllocateInfo command_buffer_allocate_info{
		.commandPool = *command_pool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = 1,
	};
	return std::move(device.allocateCommandBuffers(command_buffer_allocate_info).front());
}

void vk_record_command_buffer(
	vk::raii::CommandBuffer const &command_buffer,
	vk::raii::RenderPass const &render_pass,
	vk::raii::Framebuffer const &framebuffer,
	vk::Extent2D const &extent,
	vk::raii::Pipeline const &pipeline
) {
	vk::CommandBufferBeginInfo command_buffer_begin_info{};
	command_buffer.begin(command_buffer_begin_info);
	vk::ClearValue clear_value{
		.color{
			.float32 = std::array{0.0f, 0.0f, 0.0f, 1.0f}
		},
	};
	vk::RenderPassBeginInfo render_pass_begin_info{
		.renderPass = *render_pass,
		.framebuffer = *framebuffer,
		.renderArea{
			.offset{.x=0, .y=0},
			.extent = extent,
		},
		.clearValueCount = 1,
		.pClearValues = &clear_value,
	};
	command_buffer.beginRenderPass(render_pass_begin_info, vk::SubpassContents::eInline);
	command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
	command_buffer.draw(3, 1, 0, 0); // wow!
	command_buffer.endRenderPass(),
		command_buffer.end();
}

int main() {
	vkfw::setErrorCallback(
		[](int error_code, char const *const description) {
			std::cerr << "glfw error callback: error code " << error_code << ", " << description << std::endl;
		}
	);
	try {
		timer::start();
		auto const vkfw_instance = vkfw::initUnique();
		vkfw::UniqueWindow const vkfw_window
			= vkfw::createWindowUnique(640, 480, "window name", vkfw::WindowHints{.resizable = false});
		vk::raii::Context const vk_context;
		vk::raii::Instance const vk_instance
			= vk_create_instance(vk_context);
		// mixing normal and unique and raii is weird
		vk::raii::SurfaceKHR const vk_surface(vk_instance, vkfw::createWindowSurface(*vk_instance, *vkfw_window));
		auto const[vk_physical_device, vk_swapchain_info] = vk_choose_physical_device(vk_instance, vk_surface);
		vk::raii::Device const vk_device
			= vk_create_device(vk_physical_device);
		// create queues here? or later
		vk::raii::Queue const vk_graphics_queue(vk_device, vk_swapchain_info.get_graphics_queue_family_index(), 0);
		vk::raii::Queue const vk_present_queue(vk_device, vk_swapchain_info.get_present_queue_family_index(), 0);
		vk::Format const &vk_format = vk_swapchain_info.get_surface_format().format;
		vk::Extent2D const vk_swapchain_extent = vk_swapchain_info.get_extent(vkfw_window);
		vk::raii::SwapchainKHR const vk_swapchain
			= vk_create_swapchain(vk_device, vk_surface, vk_swapchain_info, vk_swapchain_extent); // todo refactor?
		std::vector<VkImage> const vk_images
			= vk_swapchain.getImages(); // VkImage is a handle, works differently wrt vk::Image
		std::vector<vk::raii::ImageView> const vk_image_views
			= vk_create_image_views(vk_images, vk_device, vk_format);
		// todo some of the consts must go
		vk::raii::PipelineLayout const vk_pipeline_layout
			= vk_create_pipeline_layout(vk_device);
		vk::raii::RenderPass const vk_render_pass
			= vk_create_render_pass(vk_device, vk_format);
		vk::raii::Pipeline const vk_pipeline
			= vk_create_pipeline(vk_device, vk_swapchain_extent, vk_pipeline_layout, vk_render_pass);
		std::vector<vk::raii::Framebuffer> vk_framebuffers
			= vk_create_framebuffers(vk_device, vk_image_views, vk_render_pass, vk_swapchain_extent);
		vk::raii::CommandPool vk_command_pool
			= vk_create_command_pool(vk_device, vk_swapchain_info.get_graphics_queue_family_index());
		vk::raii::CommandBuffer vk_command_buffer
			= vk_allocate_command_buffer(vk_device, vk_command_pool);

		vk::raii::Semaphore vk_image_available_semaphore = vk_device.createSemaphore({});
		vk::raii::Semaphore vk_render_finished_semaphore = vk_device.createSemaphore({});
		vk::raii::Fence vk_in_flight_fence = vk_device.createFence({.flags = vk::FenceCreateFlagBits::eSignaled});
		vk::PipelineStageFlags vk_image_available_semaphore_wait_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		timer::stop("initialization");
// todo review subpasses and synchronisation because i am tired

		while (!vkfw_window->shouldClose()) {
			if (vk::Result::eSuccess != vk_device.waitForFences({*vk_in_flight_fence}, true, std::numeric_limits<uint64_t>::max()))
				throw std::runtime_error("failed to wait for fences");
			vk_device.resetFences({*vk_in_flight_fence});
			uint32_t image_index = vk_swapchain.acquireNextImage(std::numeric_limits<uint64_t>::max(), *vk_image_available_semaphore, nullptr).second;
			vk_command_buffer.reset({});
			vk_record_command_buffer(vk_command_buffer, vk_render_pass, vk_framebuffers[image_index], vk_swapchain_extent, vk_pipeline);
			vk::SubmitInfo submit_info{
				.waitSemaphoreCount = 1,
				.pWaitSemaphores = &*vk_image_available_semaphore,
				.pWaitDstStageMask = &vk_image_available_semaphore_wait_stage,
				.commandBufferCount = 1,
				.pCommandBuffers = &*vk_command_buffer,
				.signalSemaphoreCount = 1,
				.pSignalSemaphores = &*vk_render_finished_semaphore,
			};
			vk_graphics_queue.submit({submit_info}, {*vk_in_flight_fence});
			vk::PresentInfoKHR present_info{
				.waitSemaphoreCount = 1,
				.pWaitSemaphores = &*vk_render_finished_semaphore,
				.swapchainCount = 1,
				.pSwapchains = &*vk_swapchain,
				.pImageIndices = &image_index, // todo figure this out
			};
			if (vk::Result::eSuccess != vk_present_queue.presentKHR(present_info))
				throw std::runtime_error("failed to present");
			vkfw::waitEvents();
//			vkfw::pollEvents(); // ??
		}
		vk_device.waitIdle();


	} catch (std::system_error &err) {
		std::cerr << "std::system_error: code " << err.code() << ": " << err.what() << std::endl;
		std::exit(EXIT_FAILURE);
	} catch (std::exception &err) {
		std::cerr << "std::exception: " << err.what() << std::endl;
		std::exit(EXIT_FAILURE);
	} catch (...) {
		std::cerr << "unknown error" << std::endl;
		std::exit(EXIT_FAILURE);
	}
	// todo i have no idea what to catch
}
