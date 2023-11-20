#include "Renderer.hpp"

#include "constants.hpp"

#include <array>
#include <limits>
#include <span>
#include <vector>

namespace av {
	Renderer::Renderer(size_t const &num_vertices, size_t const &num_indices)
		: vkfw_instance{vkfw::initUnique()}
		, window{framebuffer_resized}
		, instance{create_instance(context)}
		, surface{instance, vkfw::createWindowSurface(*instance, *window)}
		, gpu{instance, surface}
		, state{surface, gpu, window->getFramebufferSize()}
		, frames{constants::MAX_FRAMES_IN_FLIGHT, gpu}
		, vertex_buffer{num_vertices, num_indices, gpu, gpu.allocator} {}

	Renderer::~Renderer() {
		gpu.device.waitIdle(); // wait for Vulkan processes to finish
		// the rest should auto-destruct because RAII
	}

	bool Renderer::is_running() const {
		return !window->shouldClose();
	}

// upload vertices to the vertex _buffer
// todo this approach is probably not fast
// todo try https://redd.it/aij7zp
	/*void Renderer::set_vertices(std::vector<Vertex> const &vertices) {
		vertex_buffer.set_vertices(vertices);
	}*/

	void Renderer::draw_frame() {
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
		if (current_flight_frame == constants::MAX_FRAMES_IN_FLIGHT) current_flight_frame = 0;
	}

	vk::raii::Instance Renderer::create_instance(vk::raii::Context const &context) {
		vk::ApplicationInfo application_info{
			.pApplicationName = constants::APPLICATION_NAME,
			.apiVersion = constants::VK_API_VERSION,
		};
		auto glfw_extensions = vkfw::getRequiredInstanceExtensions();
		vk::InstanceCreateInfo instance_create_info{
			.pApplicationInfo = &application_info,
			.enabledLayerCount = static_cast<uint32_t>(constants::GLOBAL_LAYERS.size()),
			.ppEnabledLayerNames = constants::GLOBAL_LAYERS.data(),
			.enabledExtensionCount = static_cast<uint32_t>(glfw_extensions.size()),
			.ppEnabledExtensionNames = glfw_extensions.data(),
		};
		return {context, instance_create_info};
	}

	void Renderer::resize() {
		for (;;) {
			auto [width, height] = window->getFramebufferSize();
			if (width && height) break;
			vkfw::waitEvents();
			// if window is minimized, wait until it is not minimized
		}
		gpu.device.waitIdle();
		state.recreate(surface, gpu, window->getFramebufferSize());
		framebuffer_resized = false;
	}

	void Renderer::record_graphics_command_buffer(
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
}
