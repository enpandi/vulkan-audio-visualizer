//
// Created by panda on 22/05/03.
//

#include "Presenter.h"

#include <fstream>
#include <iostream>
#include <ranges>
#include <set>
// todo MORE INCLUDES MAY BE NECESSARY IDK

// many of these functions require a very specific relative ordering, which may be a source of bugs.
// todo be careful

av::Presenter::Presenter()
	: vkfw_instance{vkfw::initUnique()}
	, window{vkfw::createWindowUnique(640, 480, "window name")}
	, // todo do a config thing for these values
	instance{create_instance()}
	, surface{instance, vkfw::createWindowSurface(*instance, *window)}
	, physical_device{choose_physical_device()}
	, swapchain_info{physical_device, surface}
	, device{create_device()}
	, pipeline_layout{create_pipeline_layout()}
	, graphics_command_pool{create_command_pool()}
	, graphics_command_buffers{allocate_command_buffers()}
	, graphics_queue{device, swapchain_info.get_graphics_queue_family_index(), 0}
	, present_queue{device, swapchain_info.get_present_queue_family_index(), 0}
	, frame_sync_signalers{create_frame_sync_signalers()}
	// swapchain objects that might be recreated:
	, swapchain_format{swapchain_info.get_surface_format().format}
	, swapchain_extent{swapchain_info.get_extent(window)}
	, swapchain{create_swapchain()}
	, render_pass{create_render_pass()}
	, pipeline{create_pipeline()}
	, image_views{create_image_views()}
	, framebuffers{create_framebuffers()} {
	vkfw::setErrorCallback(
		[](int error_code, char const *const description) {
			std::cerr << "glfw error callback: error code " << error_code << ", " << description << std::endl;
		});
	window->callbacks()->on_framebuffer_resize = [&](vkfw::Window const &, size_t, size_t) {
		framebuffer_resized = true;
	};
}

av::Presenter::~Presenter() {
	device.waitIdle(); // wait for vulkan processes to finish
	// the rest should auto-destruct
}

bool av::Presenter::running() const {
	return !window->shouldClose();
}

void av::Presenter::draw_frame() {
	vk::PipelineStageFlags image_available_semaphore_wait_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	FrameSyncSignaler const &frame_sync_signaler = frame_sync_signalers[current_flight_frame];
	device.waitForFences({*frame_sync_signaler.in_flight_fence}, true, std::numeric_limits<uint64_t>::max());
	uint32_t image_index;
	try {
		image_index = swapchain.acquireNextImage(
			std::numeric_limits<uint64_t>::max(),
			*frame_sync_signaler.image_available_semaphore,
			nullptr).second;
	} catch (vk::OutOfDateKHRError const &e) {
		recreate_swapchain();
		return;
	}
	device.resetFences({*frame_sync_signaler.in_flight_fence});
	record_graphics_command_buffer(graphics_command_buffers[current_flight_frame], framebuffers[image_index]);
	vk::SubmitInfo graphics_queue_submit_info{
		.waitSemaphoreCount = 1,
		.pWaitSemaphores =  &*frame_sync_signaler.image_available_semaphore,
		.pWaitDstStageMask = &image_available_semaphore_wait_stage,
		.commandBufferCount = 1,
		.pCommandBuffers = &*graphics_command_buffers[current_flight_frame],
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &*frame_sync_signaler.render_finished_semaphore,
	};
	graphics_queue.submit({graphics_queue_submit_info}, {*frame_sync_signaler.in_flight_fence});
	vk::PresentInfoKHR present_info{
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &*frame_sync_signaler.render_finished_semaphore,
		.swapchainCount = 1,
		.pSwapchains = &*swapchain,
		.pImageIndices = &image_index,
	};
	try {
		vk::Result present_result = present_queue.presentKHR(present_info);
		if (framebuffer_resized || present_result == vk::Result::eSuboptimalKHR)
			recreate_swapchain();
	} catch (vk::OutOfDateKHRError const &e) {
		recreate_swapchain();
	}
	vkfw::pollEvents();
	++current_flight_frame;
	if (current_flight_frame == MAX_FRAMES_IN_FLIGHT) current_flight_frame = 0;
}

// delegating constructor https://stackoverflow.com/a/61033668
av::Presenter::SwapchainInfo::SwapchainInfo(
	vk::raii::PhysicalDevice const &physical_device,
	vk::raii::SurfaceKHR const &surface
) : SwapchainInfo(
	check_supports_all_extensions(physical_device),
	QueueFamilyIndices(physical_device, surface),
	physical_device.getSurfaceCapabilitiesKHR(*surface),
	choose_surface_format(physical_device, surface),
	choose_present_mode(physical_device, surface)
) {}

[[nodiscard]] bool av::Presenter::SwapchainInfo::is_compatible() const {
	return supports_all_required_extensions
	       && graphics_queue_family_index.has_value()
	       && present_queue_family_index.has_value()
	       && surface_format.has_value()
	       && present_mode.has_value();
}

// prefer value() over operator* because std::bad_optional_access seems like a useful thing to throw

[[nodiscard]] uint32_t const &av::Presenter::SwapchainInfo::get_graphics_queue_family_index() const {
	return graphics_queue_family_index.value();
}

[[nodiscard]] uint32_t const &av::Presenter::SwapchainInfo::get_present_queue_family_index() const {
	return present_queue_family_index.value();
}

[[nodiscard]] vk::SurfaceCapabilitiesKHR const &av::Presenter::SwapchainInfo::get_surface_capabilities() const {
	return surface_capabilities;
}

[[nodiscard]] vk::SurfaceFormatKHR const &av::Presenter::SwapchainInfo::get_surface_format() const {
	return surface_format.value();
}

[[nodiscard]] vk::PresentModeKHR const &av::Presenter::SwapchainInfo::get_present_mode() const {
	return present_mode.value();
}

// Window vs UniqueWindow ?
[[nodiscard]] vk::Extent2D av::Presenter::SwapchainInfo::get_extent(vkfw::UniqueWindow const &window) const {
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

bool av::Presenter::SwapchainInfo::check_supports_all_extensions(vk::raii::PhysicalDevice const &physical_device) {
	std::vector<vk::ExtensionProperties> extensions_properties = physical_device.enumerateDeviceExtensionProperties();
	return std::ranges::all_of(
		DEVICE_EXTENSIONS,
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

av::Presenter::SwapchainInfo::QueueFamilyIndices::QueueFamilyIndices(vk::raii::PhysicalDevice const &physical_device, vk::raii::SurfaceKHR const &surface) {
	std::vector<vk::QueueFamilyProperties> queue_families_properties = physical_device.getQueueFamilyProperties();
	for (uint32_t queue_family_index = 0; queue_family_index < queue_families_properties.size(); ++queue_family_index) {
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

std::optional<vk::SurfaceFormatKHR>
av::Presenter::SwapchainInfo::choose_surface_format(vk::raii::PhysicalDevice const &physical_device, vk::raii::SurfaceKHR const &surface) {
	std::vector<vk::SurfaceFormatKHR> surface_formats = physical_device.getSurfaceFormatsKHR(*surface);
	if (surface_formats.empty())
		return std::nullopt;
	for (vk::SurfaceFormatKHR const &surface_format : surface_formats)
		if (surface_format.format == vk::Format::eB8G8R8A8Srgb
		    && surface_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
			return surface_format;
	return surface_formats.front();
	// todo mess around with this
}

std::optional<vk::PresentModeKHR>
av::Presenter::SwapchainInfo::choose_present_mode(vk::raii::PhysicalDevice const &physical_device, vk::raii::SurfaceKHR const &surface) {
	std::vector<vk::PresentModeKHR> present_modes = physical_device.getSurfacePresentModesKHR(*surface);
	if (present_modes.empty())
		return std::nullopt;
	for (vk::PresentModeKHR const &present_mode : present_modes)
		if (present_mode == vk::PresentModeKHR::eMailbox)
			return present_mode;
	return vk::PresentModeKHR::eFifo;
	// todo mess around with this e.g. immediate, fifo relaxed
	// todo prefer fifo for low energy consumption
}

av::Presenter::SwapchainInfo::SwapchainInfo(
	bool const &supports_all_extensions,
	QueueFamilyIndices const &queue_family_indices,
	vk::SurfaceCapabilitiesKHR const &surface_capabilities,
	std::optional<vk::SurfaceFormatKHR> const &surface_format,
	std::optional<vk::PresentModeKHR> const &present_mode
) : supports_all_required_extensions{supports_all_extensions}
	, graphics_queue_family_index{queue_family_indices.graphics_queue_family_index}
	, present_queue_family_index{queue_family_indices.present_queue_family_index}
	, surface_capabilities{surface_capabilities}
	, surface_format{surface_format}
	, present_mode{present_mode} {}


av::Presenter::FrameSyncSignaler::FrameSyncSignaler(
	vk::raii::Device const &device,
	vk::SemaphoreCreateInfo const &image_available_semaphore_create_info,
	vk::SemaphoreCreateInfo const &render_finished_semaphore_create_info,
	vk::FenceCreateInfo const &in_flight_fence_create_info
) : image_available_semaphore{device, image_available_semaphore_create_info}
	, render_finished_semaphore{device, render_finished_semaphore_create_info}
	, in_flight_fence{device, in_flight_fence_create_info} {}


vk::raii::Instance av::Presenter::create_instance() const {
	vk::ApplicationInfo application_info{
		.pApplicationName = APPLICATION_NAME.c_str(),
		.apiVersion = VK_API_VERSION_1_1,
	};
	std::span<char const *const> glfw_extensions = vkfw::getRequiredInstanceExtensions();
	vk::InstanceCreateInfo instance_create_info{
		.pApplicationInfo = &application_info,
		.enabledLayerCount = GLOBAL_LAYERS.size(),
		.ppEnabledLayerNames = GLOBAL_LAYERS.data(),
		.enabledExtensionCount = static_cast<uint32_t>(glfw_extensions.size()),
		.ppEnabledExtensionNames = glfw_extensions.data(),
	};
	return {context, instance_create_info};
//	return context.createInstance(instance_create_info);
}

vk::raii::PhysicalDevice av::Presenter::choose_physical_device() const {
	vk::raii::PhysicalDevices physical_devices(instance);
	std::vector<av::Presenter::SwapchainInfo> swapchain_infos;
	swapchain_infos.reserve(physical_devices.size());
	std::transform(physical_devices.begin(), physical_devices.end(), std::back_inserter(swapchain_infos),
	               [&](vk::raii::PhysicalDevice const &physical_device) {
		               return av::Presenter::SwapchainInfo(physical_device, surface);
	               });
	auto usable_physical_device_indices = std::views::iota(0u, physical_devices.size())
	                                      | std::views::filter(
		[&](uint32_t const &physical_device_index) {
			return swapchain_infos[physical_device_index].is_compatible();
		});
	if (!usable_physical_device_indices)
		throw std::runtime_error("vulkan failed to find usable physical devices.");
	for (uint32_t physical_device_index : usable_physical_device_indices)
		if (physical_devices[physical_device_index].getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
			return std::move(physical_devices[physical_device_index]);
//			return {std::move(physical_devices[physical_device_index]), swapchain_infos[physical_device_index]};
	return std::move(physical_devices.front());
//	return {std::move(physical_devices.front()), swapchain_infos.front()};
}

vk::raii::Device av::Presenter::create_device() const {
	std::vector<vk::DeviceQueueCreateInfo> device_queue_create_infos;
	float queue_priority = 0.0f;
	for (uint32_t queue_family_index : std::set<uint32_t>{swapchain_info.get_graphics_queue_family_index(), swapchain_info.get_present_queue_family_index()})
		device_queue_create_infos.push_back(
			{
				.queueFamilyIndex = queue_family_index,
				.queueCount = 1,
				.pQueuePriorities = &queue_priority,
			});
	vk::PhysicalDeviceFeatures enabled_features;
	vk::DeviceCreateInfo device_create_info{
		.queueCreateInfoCount = static_cast<uint32_t>(device_queue_create_infos.size()),
		.pQueueCreateInfos = device_queue_create_infos.data(),
		.enabledLayerCount = GLOBAL_LAYERS.size(),
		.ppEnabledLayerNames = GLOBAL_LAYERS.data(),
		.enabledExtensionCount = DEVICE_EXTENSIONS.size(),
		.ppEnabledExtensionNames = DEVICE_EXTENSIONS.data(),
		.pEnabledFeatures = &enabled_features,
	};
	return {physical_device, device_create_info};
//	return physical_device.createDevice(device_create_info);
}

vk::raii::PipelineLayout av::Presenter::create_pipeline_layout() const {
	vk::PipelineLayoutCreateInfo pipeline_layout_create_info{
		.setLayoutCount = 0,
		.pushConstantRangeCount = 0,
	};
	return {device, pipeline_layout_create_info};
//	return device.createPipelineLayout(pipeline_layout_create_info);
}

vk::raii::CommandPool av::Presenter::create_command_pool() const {
	uint32_t const &graphics_queue_family_index = swapchain_info.get_graphics_queue_family_index();
	vk::CommandPoolCreateInfo command_pool_create_info{
		.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		.queueFamilyIndex = graphics_queue_family_index,
	};
	return {device, command_pool_create_info};
//	return device.createCommandPool(command_pool_create_info);
}

vk::raii::CommandBuffers av::Presenter::allocate_command_buffers() const {
	vk::CommandBufferAllocateInfo command_buffer_allocate_info{
		.commandPool = *graphics_command_pool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = MAX_FRAMES_IN_FLIGHT,
	};
	return {device, command_buffer_allocate_info};
//	return device.allocateCommandBuffers(command_buffer_allocate_info);
}

std::vector<av::Presenter::FrameSyncSignaler> av::Presenter::create_frame_sync_signalers() const {
	std::vector<FrameSyncSignaler> frame_sync_signalers;
	frame_sync_signalers.reserve(MAX_FRAMES_IN_FLIGHT);
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		frame_sync_signalers.emplace_back(
			device,
			vk::SemaphoreCreateInfo{},
			vk::SemaphoreCreateInfo{},
			vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});
	return frame_sync_signalers;
}


vk::raii::SwapchainKHR av::Presenter::create_swapchain() const {
	uint32_t const &graphics_queue_family_index = swapchain_info.get_graphics_queue_family_index();
	uint32_t const &present_queue_family_index = swapchain_info.get_present_queue_family_index();
	std::array queue_family_index_array = {graphics_queue_family_index, present_queue_family_index};
	vk::SurfaceCapabilitiesKHR const &surface_capabilities = swapchain_info.get_surface_capabilities();
	vk::SwapchainCreateInfoKHR swapchain_create_info{
		.surface = *surface,
		.minImageCount = surface_capabilities.maxImageCount == 0
		                 ? surface_capabilities.minImageCount + 1
		                 : std::min(surface_capabilities.minImageCount + 1, surface_capabilities.maxImageCount),
		.imageFormat = swapchain_info.get_surface_format().format,
		.imageColorSpace = swapchain_info.get_surface_format().colorSpace,
		.imageExtent = swapchain_extent,
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
	return {device, swapchain_create_info};
//	return device.createSwapchainKHR(swapchain_create_info);
}

vk::raii::RenderPass av::Presenter::create_render_pass() const {
	vk::AttachmentDescription attachment_description{
		.format = swapchain_format,
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
	return {device, render_pass_create_info};
//	return device.createRenderPass(render_pass_create_info);
}

vk::raii::Pipeline av::Presenter::create_pipeline() const {
	std::vector<char> vertex_shader_code = file_to_chars(VERTEX_SHADER_FILE_NAME);
	vk::raii::ShaderModule vertex_shader_module = av::Presenter::create_shader_module(vertex_shader_code);
	vk::PipelineShaderStageCreateInfo vertex_shader_stage_create_info{
		.stage = vk::ShaderStageFlagBits::eVertex, // why is this default
		.module = *vertex_shader_module,
		.pName = "main",
	};
	std::vector<char> fragment_shader_code = file_to_chars(FRAGMENT_SHADER_FILE_NAME);
	vk::raii::ShaderModule fragment_shader_module = av::Presenter::create_shader_module(fragment_shader_code);
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
		.width = static_cast<float>(swapchain_extent.width),
		.height = static_cast<float>(swapchain_extent.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};
	vk::Rect2D scissor{
		.offset{.x=0, .y=0},
		.extent = swapchain_extent,
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
	return {device, nullptr, graphics_pipeline_create_info};
//	return device.createGraphicsPipeline(nullptr, graphics_pipeline_create_info);
}

std::vector<vk::raii::ImageView> av::Presenter::create_image_views() const {
	std::vector<VkImage> images = swapchain.getImages();
	std::vector<vk::raii::ImageView> image_views;
	image_views.reserve(images.size());
	for (VkImage const &image : images) {
		vk::ImageViewCreateInfo image_view_create_info{
			.image = image,
			.viewType = vk::ImageViewType::e2D,
			.format = swapchain_format,
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

std::vector<vk::raii::Framebuffer> av::Presenter::create_framebuffers() const {
	std::vector<vk::raii::Framebuffer> framebuffers;
	framebuffers.reserve(image_views.size());
	for (vk::raii::ImageView const &image_view : image_views) {
		std::array attachments{*image_view};
		vk::FramebufferCreateInfo framebuffer_create_info{
			.renderPass = *render_pass,
			.attachmentCount = attachments.size(),
			.pAttachments = attachments.data(),
			.width = swapchain_extent.width,
			.height = swapchain_extent.height,
			.layers = 1,
		};
		framebuffers.emplace_back(device, framebuffer_create_info);
	}
	return framebuffers;
}

void av::Presenter::recreate_swapchain() {
	for (;;) {
		auto[width, height] = window->getFramebufferSize();
		if (width && height) break;
		vkfw::waitEvents();
		// if window is minimized, wait until it is not minimized
	}

	device.waitIdle();
	// placement new https://stackoverflow.com/a/54645552
	swapchain_info.~SwapchainInfo();
	new(&swapchain_info) SwapchainInfo(physical_device, surface);
	swapchain_format = swapchain_info.get_surface_format().format;
	swapchain_extent = swapchain_info.get_extent(window);
	swapchain.~SwapchainKHR(); // IMPORTANT !!
	swapchain = create_swapchain();
	render_pass = create_render_pass();
	pipeline = create_pipeline();
	image_views = create_image_views();
	framebuffers = create_framebuffers();
	framebuffer_resized = false;
}


vk::raii::ShaderModule av::Presenter::create_shader_module(std::vector<char> const &code_chars) const {
	vk::ShaderModuleCreateInfo shader_module_create_info{
		.codeSize = code_chars.size(),
		.pCode = reinterpret_cast<uint32_t const *>(code_chars.data()),
	};
	return {device, shader_module_create_info};
//	return device.createShaderModule(shader_module_create_info);
}

void av::Presenter::record_graphics_command_buffer(
	vk::raii::CommandBuffer const &command_buffer,
	vk::raii::Framebuffer const &framebuffer
) const {
	command_buffer.reset({});
	vk::CommandBufferBeginInfo command_buffer_begin_info{};
	command_buffer.begin(command_buffer_begin_info);
	vk::ClearValue clear_value{.color{.float32 = std::array{0.0f, 0.0f, 0.0f, 1.0f}}};
	vk::RenderPassBeginInfo render_pass_begin_info{
		.renderPass = *render_pass,
		.framebuffer = *framebuffer,
		.renderArea{
			.offset{.x=0, .y=0},
			.extent = swapchain_extent,
		},
		.clearValueCount = 1,
		.pClearValues = &clear_value,
	};
	command_buffer.beginRenderPass(render_pass_begin_info, vk::SubpassContents::eInline);
	command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
	command_buffer.draw(3, 1, 0, 0); // wow!
	command_buffer.endRenderPass();
	command_buffer.end();
}

std::vector<char> av::Presenter::file_to_chars(std::string const &file_name) {
	std::ifstream file(file_name, std::ios::binary | std::ios::ate);
	if (!file.is_open())
		throw std::ios::failure("failed to open file");
	std::vector<char> chars(file.tellg());
	file.seekg(0);
	file.read(chars.data(), static_cast<std::streamsize>(chars.size()));
	file.close();
	return chars;
}
