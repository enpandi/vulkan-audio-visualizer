#include "Presenter.h"

#include <fstream>
#include <iostream>
#include <ranges>
#include <set>

// todo MORE INCLUDES MAY BE NECESSARY ON OTHER PLATFORMS IDK GOTTA TEST

// function definitions are ordered the same way they are in Presenter.h (or at least they should be)

vk::VertexInputBindingDescription constexpr av::Presenter::Vertex::get_binding_description() {
	return {
		.binding = 0,
		.stride = sizeof(Vertex),
		.inputRate = vk::VertexInputRate::eVertex,
	};
}

std::array<vk::VertexInputAttributeDescription, 2> constexpr av::Presenter::Vertex::get_attribute_descriptions() {
	return {
		vk::VertexInputAttributeDescription{
			.location = 0,
			.binding = 0,
			.format = vk::Format::eR32G32Sfloat,
			.offset = offsetof(Vertex, position),
		},
		vk::VertexInputAttributeDescription{
			.location = 1,
			.binding = 0,
			.format = vk::Format::eR32G32B32Sfloat,
			.offset = offsetof(Vertex, color),
		},
	};
}

av::Presenter::Presenter(size_t const &num_vertices)
	: num_vertices{num_vertices}
	, vkfw_instance{vkfw::initUnique()}
	, window{vkfw::createWindowUnique(640, 480, "window name")} // todo do a config thing for these values
	, instance{create_instance(
		context)}
	, surface{instance, vkfw::createWindowSurface(*instance, *window)}
	, physical_device{choose_physical_device(
		instance, surface)}
	, swapchain_info{physical_device, surface, window}
	, device{create_device(
		physical_device, swapchain_info)}
	, swapchain{create_swapchain(
		device, surface, swapchain_info)}
	, pipeline_layout{create_pipeline_layout(
		device)}
	, render_pass{create_render_pass(
		device, swapchain_info)}
	, pipeline{create_pipeline(
		device, swapchain_info, pipeline_layout, render_pass)}
	, image_views{create_image_views(
		device, swapchain, swapchain_info)}
	, framebuffers{create_framebuffers(
		device, render_pass, image_views, swapchain_info)}
	, graphics_command_pool{create_command_pool(
		device, swapchain_info)}
	, graphics_command_buffers{allocate_command_buffers(
		device, graphics_command_pool)}
	, graphics_queue{device, swapchain_info.get_graphics_queue_family_index(), 0}
	, present_queue{device, swapchain_info.get_present_queue_family_index(), 0}
	, frames_sync_primitives{create_frame_sync_signalers(
		device)}
	, vertex_buffer{create_vertex_buffer(
		device, num_vertices)}
	, vertex_buffer_memory{allocate_vertex_buffer_memory(
		device, vertex_buffer, physical_device)} {
	vkfw::setErrorCallback(
		[](int error_code, char const *const description) {
			std::cerr << "glfw error callback: error code " << error_code << ", " << description << std::endl;
		});
	window->callbacks()->on_framebuffer_resize = [&](vkfw::Window const &, size_t, size_t) {
		framebuffer_resized = true;
	};
	bind_vertex_buffer_memory();
}

av::Presenter::~Presenter() {
	device.waitIdle(); // wait for vulkan processes to finish
	// the rest should auto-destruct
}

bool av::Presenter::is_running() const {
	return !window->shouldClose();
}

// upload vertices to the vertex buffer
// todo this approach is probably not fast
// todo try https://redd.it/aij7zp
void av::Presenter::set_vertices(Vertex const *const vertices) const {
	void *data = vertex_buffer_memory.mapMemory(0, num_vertices * sizeof(Vertex), vk::MemoryMapFlags{});
	memcpy(data, vertices, num_vertices * sizeof(Vertex));
	vertex_buffer_memory.unmapMemory();
}

void av::Presenter::draw_frame() {
	vk::PipelineStageFlags image_available_semaphore_wait_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	FrameSyncPrimitives const &frame_sync_primitives = frames_sync_primitives[current_flight_frame];
	device.waitForFences({*frame_sync_primitives.frame_in_flight}, true, std::numeric_limits<uint64_t>::max());
	uint32_t image_index;
	try {
		image_index = swapchain.acquireNextImage(
			std::numeric_limits<uint64_t>::max(),
			*frame_sync_primitives.draw_complete,
			nullptr).second;
	} catch (vk::OutOfDateKHRError const &e) {
		recreate_swapchain();
		return;
	}
	device.resetFences({*frame_sync_primitives.frame_in_flight});
	record_graphics_command_buffer(graphics_command_buffers[current_flight_frame], framebuffers[image_index]);
	vk::SubmitInfo graphics_queue_submit_info{
		.waitSemaphoreCount = 1,
		.pWaitSemaphores =  &*frame_sync_primitives.draw_complete,
		.pWaitDstStageMask = &image_available_semaphore_wait_stage,
		.commandBufferCount = 1,
		.pCommandBuffers = &*graphics_command_buffers[current_flight_frame],
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &*frame_sync_primitives.present_complete,
	};
	graphics_queue.submit({graphics_queue_submit_info}, {*frame_sync_primitives.frame_in_flight});
	vk::PresentInfoKHR present_info{
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &*frame_sync_primitives.present_complete,
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

av::Presenter::QueueFamilyIndices::QueueFamilyIndices(vk::raii::PhysicalDevice const &physical_device, vk::raii::SurfaceKHR const &surface) {
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

[[nodiscard]] bool av::Presenter::QueueFamilyIndices::is_compatible() const {
	return graphics_queue_family_index.has_value() && present_queue_family_index.has_value();
}

// prefer value() over operator* because std::bad_optional_access seems like a useful thing to throw

[[nodiscard]] uint32_t const &av::Presenter::QueueFamilyIndices::get_graphics_queue_family_index() const {
	return graphics_queue_family_index.value();
}

[[nodiscard]] uint32_t const &av::Presenter::QueueFamilyIndices::get_present_queue_family_index() const {
	return present_queue_family_index.value();
}

// delegating constructor https://stackoverflow.com/a/61033668
av::Presenter::SwapchainInfo::SwapchainInfo(
	vk::raii::PhysicalDevice const &physical_device,
	vk::raii::SurfaceKHR const &surface,
	vkfw::UniqueWindow const &window
) : SwapchainInfo(
	check_supports_all_extensions(physical_device),
	QueueFamilyIndices(physical_device, surface),
	physical_device.getSurfaceCapabilitiesKHR(*surface),
	choose_surface_format(physical_device, surface),
	choose_present_mode(physical_device, surface),
	window
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
[[nodiscard]] vk::Extent2D av::Presenter::SwapchainInfo::get_extent() const {
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
	std::optional<vk::PresentModeKHR> const &present_mode,
	vkfw::UniqueWindow const &window
) : supports_all_required_extensions{supports_all_extensions}
	, graphics_queue_family_index{queue_family_indices.get_graphics_queue_family_index()}
	, present_queue_family_index{queue_family_indices.get_present_queue_family_index()}
	, surface_capabilities{surface_capabilities}
	, surface_format{surface_format}
	, present_mode{present_mode}
	, window{window} {}


av::Presenter::FrameSyncPrimitives::FrameSyncPrimitives(
	vk::raii::Device const &device,
	vk::SemaphoreCreateInfo const &image_available_semaphore_create_info,
	vk::SemaphoreCreateInfo const &render_finished_semaphore_create_info,
	vk::FenceCreateInfo const &in_flight_fence_create_info
) : draw_complete{device, image_available_semaphore_create_info}
	, present_complete{device, render_finished_semaphore_create_info}
	, frame_in_flight{device, in_flight_fence_create_info} {}


vk::raii::Instance av::Presenter::create_instance(vk::raii::Context const &context) {
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

vk::raii::PhysicalDevice av::Presenter::choose_physical_device(vk::raii::Instance const &instance, vk::raii::SurfaceKHR const &surface) {
	vk::raii::PhysicalDevices physical_devices(instance);
	std::vector<av::Presenter::QueueFamilyIndices> queue_families_indices;
//	std::vector<av::Presenter::SwapchainInfo> swapchain_infos;
	queue_families_indices.reserve(physical_devices.size());
	std::transform(physical_devices.begin(), physical_devices.end(), std::back_inserter(queue_families_indices),
	               [&](vk::raii::PhysicalDevice const &physical_device) {
		               return av::Presenter::QueueFamilyIndices(physical_device, surface);
	               });
	auto usable_physical_device_indices = std::views::iota(0u, physical_devices.size())
	                                      | std::views::filter(
		[&](uint32_t const &physical_device_index) {
			return queue_families_indices[physical_device_index].is_compatible();
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

vk::raii::Device av::Presenter::create_device(vk::raii::PhysicalDevice const &physical_device, av::Presenter::SwapchainInfo const &swapchain_info) {
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

vk::raii::SwapchainKHR av::Presenter::create_swapchain(
	vk::raii::Device const &device,
	vk::raii::SurfaceKHR const &surface,
	av::Presenter::SwapchainInfo const &swapchain_info
) {
	uint32_t const &graphics_queue_family_index = swapchain_info.get_graphics_queue_family_index();
	uint32_t const &present_queue_family_index = swapchain_info.get_present_queue_family_index();
	vk::SurfaceCapabilitiesKHR const &surface_capabilities = swapchain_info.get_surface_capabilities();
	vk::PresentModeKHR const &present_mode = swapchain_info.get_present_mode();
	std::array queue_family_index_array = {
		swapchain_info.get_graphics_queue_family_index(),
		swapchain_info.get_present_queue_family_index()
	};
	vk::SwapchainCreateInfoKHR swapchain_create_info{
		.surface = *surface,
		.minImageCount = surface_capabilities.maxImageCount == 0
		                 ? surface_capabilities.minImageCount + 1
		                 : std::min(surface_capabilities.minImageCount + 1, surface_capabilities.maxImageCount),
		.imageFormat = swapchain_info.get_surface_format().format,
		.imageColorSpace = swapchain_info.get_surface_format().colorSpace,
		.imageExtent = swapchain_info.get_extent(),
		.imageArrayLayers = 1,
		.imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
		.imageSharingMode = graphics_queue_family_index == present_queue_family_index ?
		                    vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent,
		.queueFamilyIndexCount = queue_family_index_array.size(),
		.pQueueFamilyIndices = queue_family_index_array.data(),
		.preTransform = surface_capabilities.currentTransform, // default???
		.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque, // default
		.presentMode = present_mode,
		.clipped = true,
//		.oldSwapchain = VK_NULL_HANDLE, // ???
	};
	return {device, swapchain_create_info};
//	return device.createSwapchainKHR(swapchain_create_info);
}

vk::raii::PipelineLayout av::Presenter::create_pipeline_layout(vk::raii::Device const &device) {
	vk::PipelineLayoutCreateInfo pipeline_layout_create_info{
		.setLayoutCount = 0,
		.pushConstantRangeCount = 0,
	};
	return {device, pipeline_layout_create_info};
//	return device.createPipelineLayout(pipeline_layout_create_info);
}

vk::raii::RenderPass av::Presenter::create_render_pass(vk::raii::Device const &device, av::Presenter::SwapchainInfo const &swapchain_info) {
	vk::Format const &swapchain_format = swapchain_info.get_surface_format().format;
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

vk::raii::Pipeline av::Presenter::create_pipeline(
	vk::raii::Device const &device,
	av::Presenter::SwapchainInfo const &swapchain_info,
	vk::raii::PipelineLayout const &pipeline_layout,
	vk::raii::RenderPass const &render_pass
) {
	std::vector<char> vertex_shader_code = file_to_chars(VERTEX_SHADER_FILE_NAME);
	vk::raii::ShaderModule vertex_shader_module = av::Presenter::create_shader_module(device, vertex_shader_code);
	vk::PipelineShaderStageCreateInfo vertex_shader_stage_create_info{
		.stage = vk::ShaderStageFlagBits::eVertex, // why is this default
		.module = *vertex_shader_module,
		.pName = "main",
	};
	std::vector<char> fragment_shader_code = file_to_chars(FRAGMENT_SHADER_FILE_NAME);
	vk::raii::ShaderModule fragment_shader_module = av::Presenter::create_shader_module(device, fragment_shader_code);
	vk::PipelineShaderStageCreateInfo fragment_shader_stage_info{
		.stage = vk::ShaderStageFlagBits::eFragment,
		.module = *fragment_shader_module,
		.pName = "main",
	};
	std::array pipeline_shader_stage_create_infos{vertex_shader_stage_create_info, fragment_shader_stage_info};
	vk::VertexInputBindingDescription vertex_input_binding_description = Vertex::get_binding_description();
	std::array vertex_input_attribute_descriptions = Vertex::get_attribute_descriptions();
	vk::PipelineVertexInputStateCreateInfo pipeline_vertex_input_state_create_info{
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &vertex_input_binding_description,
		.vertexAttributeDescriptionCount = vertex_input_attribute_descriptions.size(),
		.pVertexAttributeDescriptions = vertex_input_attribute_descriptions.data(),
	}; // todo
	vk::PipelineInputAssemblyStateCreateInfo pipeline_input_assembly_state_create_info{
		.topology = vk::PrimitiveTopology::eTriangleStrip,
		.primitiveRestartEnable = false,
	};
//	vk::PipelineTessellationStateCreateInfo tesselation_state_info{};
	vk::Extent2D const &swapchain_extent = swapchain_info.get_extent();
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
//		.depthClampEnable = false,
//		.rasterizerDiscardEnable = false,
		.polygonMode = vk::PolygonMode::eFill, // todo mess with these settings
//		.cullMode = vk::CullModeFlagBits::eNone, // todo take advantage of culling and tristrip when drawing the spectrum
//		.frontFace = vk::FrontFace::eClockwise,
//		.depthBiasEnable = false,
		.lineWidth = 1.0f,
	};
	vk::PipelineMultisampleStateCreateInfo pipeline_multisample_state_create_info{
		.rasterizationSamples = vk::SampleCountFlagBits::e1, // default
		.sampleShadingEnable = false, // hmm
	};
	vk::PipelineColorBlendAttachmentState pipeline_color_blend_attachment_state{
//		.blendEnable = false,
		.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
	};
	vk::PipelineColorBlendStateCreateInfo pipeline_color_blend_state_create_info{
//		.logicOpEnable = false,
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

std::vector<vk::raii::ImageView>
av::Presenter::create_image_views(vk::raii::Device const &device, vk::raii::SwapchainKHR const &swapchain, av::Presenter::SwapchainInfo const &swapchain_info) {
	std::vector<VkImage> images = swapchain.getImages();
	vk::Format const &swapchain_format = swapchain_info.get_surface_format().format;
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

std::vector<vk::raii::Framebuffer> av::Presenter::create_framebuffers(
	vk::raii::Device const &device,
	vk::raii::RenderPass const &render_pass,
	std::vector<vk::raii::ImageView> const &image_views,
	av::Presenter::SwapchainInfo const &swapchain_info
) {
	vk::Extent2D const &swapchain_extent = swapchain_info.get_extent();
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

vk::raii::CommandPool av::Presenter::create_command_pool(vk::raii::Device const &device, av::Presenter::SwapchainInfo const &swapchain_info) {
	uint32_t const &graphics_queue_family_index = swapchain_info.get_graphics_queue_family_index();
	vk::CommandPoolCreateInfo command_pool_create_info{
		.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		.queueFamilyIndex = graphics_queue_family_index,
	};
	return {device, command_pool_create_info};
//	return device.createCommandPool(command_pool_create_info);
}

vk::raii::CommandBuffers av::Presenter::allocate_command_buffers(vk::raii::Device const &device, vk::raii::CommandPool const &graphics_command_pool) {
	vk::CommandBufferAllocateInfo command_buffer_allocate_info{
		.commandPool = *graphics_command_pool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = MAX_FRAMES_IN_FLIGHT,
	};
	return {device, command_buffer_allocate_info};
//	return device.allocateCommandBuffers(command_buffer_allocate_info);
}

std::vector<av::Presenter::FrameSyncPrimitives> av::Presenter::create_frame_sync_signalers(vk::raii::Device const &device) {
	std::vector<FrameSyncPrimitives> frame_sync_signalers;
	frame_sync_signalers.reserve(MAX_FRAMES_IN_FLIGHT);
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		frame_sync_signalers.emplace_back(
			device,
			vk::SemaphoreCreateInfo{},
			vk::SemaphoreCreateInfo{},
			vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});
	return frame_sync_signalers;
}

vk::raii::ShaderModule av::Presenter::create_shader_module(vk::raii::Device const &device, std::vector<char> const &code_chars) {
	vk::ShaderModuleCreateInfo shader_module_create_info{
		.codeSize = code_chars.size(),
		.pCode = reinterpret_cast<uint32_t const *>(code_chars.data()),
	};
	return {device, shader_module_create_info};
//	return device.createShaderModule(shader_module_create_info);
}

vk::raii::Buffer av::Presenter::create_vertex_buffer(vk::raii::Device const &device, size_t const &num_vertices) {
	std::cout << num_vertices << std::endl;
	vk::BufferCreateInfo buffer_create_info{
		.size = num_vertices * sizeof(Vertex),
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
	vk::MemoryType x;
	vk::PhysicalDeviceMemoryProperties memory_properties = physical_device.getMemoryProperties();
	for (uint32_t memory_type_index = 0; memory_type_index < memory_properties.memoryTypeCount; ++memory_type_index)
		if ((memory_type_bits & (1 << memory_type_index))
		    && !(~memory_properties.memoryTypes[memory_type_index].propertyFlags & memory_property_flags))
			return memory_type_index;
	return std::nullopt;
}

vk::raii::DeviceMemory av::Presenter::allocate_vertex_buffer_memory(
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


void av::Presenter::bind_vertex_buffer_memory() const {
	vertex_buffer.bindMemory(*vertex_buffer_memory, 0);
//	void *data = vertex_buffer_memory.mapMemory(0, num_vertices * sizeof(Vertex), vk::MemoryMapFlags{});
//	memcpy(data, VERTICES.data(), num_vertices * sizeof(Vertex));
//	vertex_buffer_memory.unmapMemory();
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
	new(&swapchain_info) SwapchainInfo(physical_device, surface, window);
	swapchain.~SwapchainKHR(); // IMPORTANT !!
	swapchain = create_swapchain(device, surface, swapchain_info);
	render_pass = create_render_pass(device, swapchain_info);
	pipeline = create_pipeline(device, swapchain_info, pipeline_layout, render_pass); // todo make sure the dtors of the old objects actually get called
	image_views = create_image_views(device, swapchain, swapchain_info);
	framebuffers = create_framebuffers(device, render_pass, image_views, swapchain_info);
	framebuffer_resized = false;
}


void av::Presenter::record_graphics_command_buffer(
	vk::raii::CommandBuffer const &command_buffer,
	vk::raii::Framebuffer const &framebuffer
) const {
	vk::Extent2D const &swapchain_extent = swapchain_info.get_extent();
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
	command_buffer.bindVertexBuffers(0, {*vertex_buffer}, {0});
//	command_buffer.draw(3, 1, 0, 0); // wow!
	command_buffer.draw(num_vertices, 1, 0, 0);
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