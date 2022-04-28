#include <limits>
#include <cstdint>
#include <iostream>
#include <array>
#include <ranges>
#include <algorithm>
#include <utility>
#include <functional>
#include <fstream>
#include <ios>

#define VULKAN_HPP_NO_CONSTRUCTORS
#define VKFW_NO_STRUCT_CONSTRUCTORS
#include <vkfw/vkfw.hpp>
#include <vulkan/vulkan_raii.hpp>

// todo disable stuff in release mode, for now assume debug mode always
static std::array const vk_global_layers = {"VK_LAYER_KHRONOS_validation"}; // todo is there a macro for this, or:
static std::array const vk_device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME}; // todo is there a vulkan-hpp constant for this
static std::string const application_name = "audio visualizer";
static std::string const vertex_shader_file_name = "shaders/shader.vert.spv";
static std::string const fragment_shader_file_name = "shaders/shader.frag.spv";

struct QueueFamilyIndices {
	std::optional<uint32_t> graphics_queue_family_index;
	std::optional<uint32_t> present_queue_family_index;

	QueueFamilyIndices(vk::raii::PhysicalDevice const &physical_device, vk::raii::SurfaceKHR const &surface) {
		std::vector<vk::QueueFamilyProperties> queue_family_properties = physical_device.getQueueFamilyProperties();
		auto is_graphics_supported = [&](uint32_t queue_family_index) {
			return static_cast<bool>(queue_family_properties[queue_family_index].queueFlags & vk::QueueFlagBits::eGraphics);
		};
		auto is_surface_supported = [&](uint32_t queue_family_index) {
			return static_cast<bool>(physical_device.getSurfaceSupportKHR(queue_family_index, *surface));
		};
		auto usable_queue_families = std::views::iota(0u, queue_family_properties.size())
		                             | std::views::filter(is_graphics_supported)
		                             | std::views::filter(is_surface_supported);
		if (!std::ranges::empty(usable_queue_families)) {
			graphics_queue_family_index = usable_queue_families.front();
			present_queue_family_index = usable_queue_families.front();
		} else {
			auto graphics_queue_families = std::views::iota(0u, queue_family_properties.size())
			                               | std::views::filter(is_graphics_supported);
			if (!std::ranges::empty(graphics_queue_families))
				graphics_queue_family_index = graphics_queue_families.front();

			auto present_queue_families = std::views::iota(0u, queue_family_properties.size())
			                              | std::views::filter(is_surface_supported);
			if (!std::ranges::empty(present_queue_families))
				present_queue_family_index = present_queue_families.front();
		}
	}

	[[nodiscard]] bool has_all_indices() const {
		return graphics_queue_family_index.has_value() && present_queue_family_index.has_value();
	}
};

struct SurfaceInformation {
	std::vector<vk::SurfaceFormatKHR> surface_formats;
	vk::SurfaceCapabilitiesKHR surface_capabilities;
	std::vector<vk::PresentModeKHR> present_modes;

	SurfaceInformation(vk::raii::PhysicalDevice const &physical_device, vk::raii::SurfaceKHR const &surface) :
		surface_formats{
			physical_device.getSurfaceFormatsKHR(*surface)
		},
		surface_capabilities{
			physical_device.getSurfaceCapabilitiesKHR(*surface)
		},
		present_modes{
			physical_device.getSurfacePresentModesKHR(*surface)
		} {}

	// todo for these functions, & or no & ?
	[[nodiscard]] vk::SurfaceFormatKHR choose_surface_format() const {
		if (surface_formats.empty())
			throw std::runtime_error("there weren't any surface formats to choose from.");
		// should this be std::out_of_range ?
		for (vk::SurfaceFormatKHR const &surface_format: surface_formats) {
			if (surface_format.format == vk::Format::eB8G8R8A8Srgb
			    && surface_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
				return surface_format;
		}
		return surface_formats.front();
		// todo mess around with this
	}

	// vkfw::UniqueWindow vs vkfw::Window ?
	[[nodiscard]] vk::Extent2D choose_extent(vkfw::UniqueWindow const &window) const {
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

	[[nodiscard]] vk::PresentModeKHR choose_present_mode() const {
		if (present_modes.empty())
			throw std::runtime_error("there weren't any present modes to choose from.");
		// should this be std::out_of_range ?
		for (vk::PresentModeKHR const &present_mode: present_modes) {
			if (present_mode == vk::PresentModeKHR::eMailbox)
				return present_mode;
		}
		return vk::PresentModeKHR::eFifo;
		// todo mess around with this e.g. immediate, fifo relaxed
		// todo prefer fifo for low energy consumption
	}
};

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

vk::raii::PhysicalDevice vk_choose_physical_device(vk::raii::Instance const &instance, vk::raii::SurfaceKHR const &surface) {
	vk::raii::PhysicalDevices physical_devices(instance);
	auto usable_physical_devices = std::views::all(physical_devices)
	                               | std::views::filter(
		[&](vk::raii::PhysicalDevice const &physical_device) {
			bool queue_families_present = QueueFamilyIndices(physical_device, surface).has_all_indices();
			std::vector<vk::ExtensionProperties> extensions_properties = physical_device.enumerateDeviceExtensionProperties();
			bool device_extensions_supported = std::ranges::all_of(
				vk_device_extensions,
				[&](std::string const &device_extension) {
					return std::ranges::any_of(
						extensions_properties,
						[&](vk::ExtensionProperties const &extension_properties) {
							return device_extension == extension_properties.extensionName;
						});
				});
			SurfaceInformation surface_information(physical_device, surface);
			bool surface_ok = !surface_information.surface_formats.empty() && !surface_information.present_modes.empty();
			return queue_families_present && device_extensions_supported && surface_ok;
		});
	if (std::ranges::empty(usable_physical_devices))
		throw std::runtime_error("vulkan failed to find usable physical devices.");
	return std::move(*std::ranges::max_element(
		usable_physical_devices,
		[](vk::raii::PhysicalDevice const &physical_device_1, vk::raii::PhysicalDevice const &physical_device_2) {
			return (physical_device_1.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
			       <
			       (physical_device_2.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu);
		}));
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
	vk::raii::PhysicalDevice const &physical_device, vk::raii::SurfaceKHR const &surface, vkfw::UniqueWindow const &window
) {
	SurfaceInformation surface_information(physical_device, surface);
	vk::SurfaceFormatKHR surface_format = surface_information.choose_surface_format();
	QueueFamilyIndices queue_family_indices(physical_device, surface);
	std::array queue_family_index_array{*queue_family_indices.graphics_queue_family_index, *queue_family_indices.present_queue_family_index};
	vk::SwapchainCreateInfoKHR swapchain_create_info{
		.surface = *surface,
		.minImageCount = surface_information.surface_capabilities.maxImageCount == 0
		                 ? surface_information.surface_capabilities.minImageCount + 1
		                 : std::min(surface_information.surface_capabilities.minImageCount + 1, surface_information.surface_capabilities.maxImageCount),
		.imageFormat = surface_format.format,
		.imageColorSpace = surface_format.colorSpace,
		.imageExtent = surface_information.choose_extent(window),
		.imageArrayLayers = 1,
		.imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
		.imageSharingMode = *queue_family_indices.graphics_queue_family_index == *queue_family_indices.present_queue_family_index ?
		                    vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent,
		.queueFamilyIndexCount = queue_family_index_array.size(),
		.pQueueFamilyIndices = queue_family_index_array.data(),
		.preTransform = surface_information.surface_capabilities.currentTransform, // default???
		.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque, // default
		.presentMode = surface_information.choose_present_mode(),
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
	std::cout << code_chars.size() << std::endl;
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
		.blendEnable = false, // todo what if it's false? ??????????????????????UHHHHH
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

		auto const vkfw_instance = vkfw::initUnique();
		vkfw::UniqueWindow const vkfw_window
			= vkfw::createWindowUnique(640, 480, "window name", vkfw::WindowHints{.resizable = false});
		vk::raii::Context const vk_context;
		vk::raii::Instance const vk_instance
			= vk_create_instance(vk_context);
		// mixing normal and unique and raii is weird
		vk::raii::SurfaceKHR const vk_surface(vk_instance, vkfw::createWindowSurface(*vk_instance, *vkfw_window));
		vk::raii::PhysicalDevice const vk_physical_device
			= vk_choose_physical_device(vk_instance, vk_surface);
		vk::raii::Device const vk_device
			= vk_create_device(vk_physical_device);
		// create queues here? or later
		QueueFamilyIndices queue_family_indices(vk_physical_device, vk_surface);
		vk::raii::Queue const vk_graphics_queue(vk_device, *queue_family_indices.graphics_queue_family_index, 0);
		vk::raii::Queue const vk_present_queue(vk_device, *queue_family_indices.present_queue_family_index, 0);
		vk::raii::SwapchainKHR const vk_swapchain
			= vk_create_swapchain(vk_device, vk_physical_device, vk_surface, vkfw_window); // todo refactor?
		// todo "we'll need the format and extent in future chapters" (https://vulkan-tutorial.com/en/Drawing_a_triangle/Presentation/Swap_chain)
		std::vector<VkImage> const vk_images
			= vk_swapchain.getImages(); // VkImage is a handle, works differently wrt vk::Image
		vk::Format const vk_format
			= SurfaceInformation(vk_physical_device, vk_surface).choose_surface_format().format; // todo i have a lot of redundancy
		vk::Extent2D const vk_extent
			= SurfaceInformation(vk_physical_device, vk_surface).choose_extent(vkfw_window); // todo reduce the redundancy in the computations
		std::vector<vk::raii::ImageView> const vk_image_views
			= vk_create_image_views(vk_images, vk_device, vk_format);
		// todo some of the consts must go
		vk::raii::PipelineLayout const vk_pipeline_layout
			= vk_create_pipeline_layout(vk_device);
		vk::raii::RenderPass const vk_render_pass
			= vk_create_render_pass(vk_device, vk_format);
		vk::raii::Pipeline const vk_pipeline
			= vk_create_pipeline(vk_device, vk_extent, vk_pipeline_layout, vk_render_pass);
		std::vector<vk::raii::Framebuffer> vk_framebuffers
			= vk_create_framebuffers(vk_device, vk_image_views, vk_render_pass, vk_extent);
		vk::raii::CommandPool vk_command_pool
			= vk_create_command_pool(vk_device, *queue_family_indices.graphics_queue_family_index);
		vk::raii::CommandBuffer vk_command_buffer
			= vk_allocate_command_buffer(vk_device, vk_command_pool);

		vk::raii::Semaphore vk_image_available_semaphore = vk_device.createSemaphore({});
		vk::raii::Semaphore vk_render_finished_semaphore = vk_device.createSemaphore({});
		vk::raii::Fence vk_in_flight_fence = vk_device.createFence({.flags = vk::FenceCreateFlagBits::eSignaled});
		vk::PipelineStageFlags vk_image_available_semaphore_wait_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
// todo review subpasses and synchronisation because i am tired

		while (!vkfw_window->shouldClose()) {
			if (vk::Result::eSuccess != vk_device.waitForFences({*vk_in_flight_fence}, true, std::numeric_limits<uint64_t>::max()))
				throw std::runtime_error("failed to wait for fences");
			vk_device.resetFences({*vk_in_flight_fence});
			uint32_t image_index = vk_swapchain.acquireNextImage(std::numeric_limits<uint64_t>::max(), *vk_image_available_semaphore, nullptr).second;
			vk_command_buffer.reset({});
			vk_record_command_buffer(vk_command_buffer, vk_render_pass, vk_framebuffers[image_index], vk_extent, vk_pipeline);
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
			vkfw::pollEvents();
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
