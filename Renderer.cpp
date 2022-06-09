#include "Renderer.hpp"

#include "constants.hpp"

#include <array>
#include <limits>
#include <span>
#include <vector>

// todo MORE INCLUDES MAY BE NECESSARY ON OTHER PLATFORMS IDK GOTTA TEST

av::Renderer::Renderer(size_t const &num_vertices)
	: window{framebuffer_resized}
	, instance{create_instance(context)}
	, surface{instance, vkfw::createWindowSurface(*instance, *window)}
	, gpu{instance, surface}
	, state{surface, gpu, window->getFramebufferSize()}
	, allocator{create_allocator(gpu, instance)}
	, vertex_buffer{num_vertices, gpu, allocator}
	, frames{constants::MAX_FRAMES_IN_FLIGHT, gpu} {}

av::Renderer::~Renderer() {
	gpu.device.waitIdle(); // wait for Vulkan processes to finish
	// the rest should auto-destruct because RAII
}

bool av::Renderer::is_running() const {
	return !window->shouldClose();
}

// upload vertices to the vertex buffer
// todo this approach is probably not fast
// todo try https://redd.it/aij7zp
void av::Renderer::set_vertices(std::vector<Vertex> const &vertices) {
	vertex_buffer.set_vertices(vertices);
}

void av::Renderer::draw_frame() {
	vk::PipelineStageFlags image_available_semaphore_wait_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	Frame const &frame = frames[current_flight_frame];
	gpu.device.waitForFences({*frame.in_flight}, true, std::numeric_limits<uint64_t>::max());
	uint32_t image_index;
	try {
		image_index = state.swapchain.acquireNextImage(
			std::numeric_limits<uint64_t>::max(),
			*frame.draw_complete, nullptr
		).second;
	} catch (vk::OutOfDateKHRError const &e) {
		resize();
		return;
	}
	Framebuffer const &framebuffer = state.framebuffers[image_index];
	gpu.device.resetFences({*frame.in_flight});
	record_graphics_command_buffer(frame.command_buffer, framebuffer.framebuffer);
	vk::SubmitInfo graphics_queue_submit_info{
		.waitSemaphoreCount = 1,
		.pWaitSemaphores =  &*frame.draw_complete,
		.pWaitDstStageMask = &image_available_semaphore_wait_stage,
		.commandBufferCount = 1,
		.pCommandBuffers = &*frame.command_buffer,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &*frame.present_complete,
	};
	gpu.graphics_queue.submit({graphics_queue_submit_info}, {*frame.in_flight});
	vk::PresentInfoKHR present_info{
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &*frame.present_complete,
		.swapchainCount = 1,
		.pSwapchains = &*state.swapchain,
		.pImageIndices = &image_index,
	};
	try {
		vk::Result present_result = gpu.present_queue.presentKHR(present_info);
		if (framebuffer_resized || present_result == vk::Result::eSuboptimalKHR)
			resize();
	} catch (vk::OutOfDateKHRError const &e) {
		resize();
	}
	vkfw::pollEvents();
	++current_flight_frame;
	if (current_flight_frame == av::constants::MAX_FRAMES_IN_FLIGHT) current_flight_frame = 0;
}

vk::raii::Instance av::Renderer::create_instance(vk::raii::Context const &context) {
	vk::ApplicationInfo application_info{
		.pApplicationName = av::constants::APPLICATION_NAME,
		.apiVersion = av::constants::VK_API_VERSION,
	};
	auto glfw_extensions = vkfw::getRequiredInstanceExtensions();
	vk::InstanceCreateInfo instance_create_info{
		.pApplicationInfo = &application_info,
		.enabledLayerCount = static_cast<uint32_t>(av::constants::GLOBAL_LAYERS.size()),
		.ppEnabledLayerNames = av::constants::GLOBAL_LAYERS.data(),
		.enabledExtensionCount = static_cast<uint32_t>(glfw_extensions.size()),
		.ppEnabledExtensionNames = glfw_extensions.data(),
	};
	return {context, instance_create_info};
}

vma::Allocator av::Renderer::create_allocator(GraphicsDevice const &gpu, vk::raii::Instance const &instance) {
	vma::AllocatorCreateInfo allocator_create_info{
		.physicalDevice = *gpu.physical_device,
		.device = *gpu.device,
		.instance = *instance,
		.vulkanApiVersion = constants::VK_API_VERSION,
	};
	return vma::createAllocator(allocator_create_info);
}

void av::Renderer::resize() {
	for (;;) {
		auto[width, height] = window->getFramebufferSize();
		if (width && height) break;
		vkfw::waitEvents();
		// if window is minimized, wait until it is not minimized
	}
	state.recreate(surface, gpu, window->getFramebufferSize());
	framebuffer_resized = false;
}

void av::Renderer::record_graphics_command_buffer(
	vk::raii::CommandBuffer const &command_buffer,
	vk::raii::Framebuffer const &framebuffer
) const {
	command_buffer.reset({});
	command_buffer.begin({});
	{
		vk::ClearValue clear_value{.color{.float32 = std::array{0.0f, 0.0f, 0.0f, 1.0f}}};
		vk::RenderPassBeginInfo render_pass_begin_info{
			.renderPass = *state.render_pass,
			.framebuffer = *framebuffer,
			.renderArea{
				.offset{.x=0, .y=0},
				.extent = state.surface_info.extent,
			},
			.clearValueCount = 1,
			.pClearValues = &clear_value,
		};
		command_buffer.beginRenderPass(render_pass_begin_info, vk::SubpassContents::eInline);
		{
			command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *state.pipeline);
			vertex_buffer.bind_and_draw(command_buffer);
		}
		command_buffer.endRenderPass();
	}
	command_buffer.end();
}
