#include "constants.hpp"

#include "GraphicsState.hpp"
#include "Vertex.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <ios>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace av {
	GraphicsState::GraphicsState(
		vk::raii::SurfaceKHR const &surface,
		GraphicsDevice const &gpu,
		std::tuple<size_t, size_t> const &framebuffer_size
	)
		: vertex_shader_module{create_shader_module(constants::VERTEX_SHADER_FILE_NAME, gpu)}
		, fragment_shader_module{create_shader_module(constants::FRAGMENT_SHADER_FILE_NAME, gpu)}
		, pipeline_layout{create_pipeline_layout(gpu)}
		, _surface_info{surface, gpu, framebuffer_size}
		, _swapchain{create_swapchain(surface, gpu, surface_info, nullptr)}
		, _render_pass{create_render_pass(gpu, surface_info)}
		, _pipeline{create_pipeline(
			gpu, vertex_shader_module, fragment_shader_module, pipeline_layout, surface_info, render_pass)}
		, _framebuffers{gpu, surface_info, swapchain, render_pass} {}


	void GraphicsState::recreate(
		vk::raii::SurfaceKHR const &surface,
		GraphicsDevice const &gpu,
		std::tuple<size_t, size_t> const &framebuffer_size
	) {
		gpu.device.waitIdle();
		// placement new https://stackoverflow.com/a/54645552
		std::destroy_at(&_surface_info);
		std::construct_at(&_surface_info, surface, gpu, framebuffer_size);
		_swapchain = create_swapchain(surface, gpu, _surface_info, _swapchain);
		_render_pass = create_render_pass(gpu, _surface_info);
		_pipeline = create_pipeline(
			gpu, vertex_shader_module, fragment_shader_module, pipeline_layout, _surface_info, _render_pass);
		_framebuffers = Framebuffers(gpu, surface_info, swapchain, render_pass);
	}

	vk::raii::ShaderModule GraphicsState::create_shader_module(
		std::string const &file_name,
		GraphicsDevice const &gpu
	) {
		std::ifstream file(file_name, std::ios::binary | std::ios::ate);
		if (!file.is_open())
			throw std::ios::failure("failed to open file");
		std::vector<char> bytecode(file.tellg());
		file.seekg(0);
		file.read(bytecode.data(), static_cast<std::streamsize>(bytecode.size()));
		file.close();
		vk::ShaderModuleCreateInfo shader_module_create_info{
			.codeSize = bytecode.size(),
			.pCode = reinterpret_cast<uint32_t const *>(bytecode.data()),
		};
		return {gpu.device, shader_module_create_info};
	}

	vk::raii::PipelineLayout GraphicsState::create_pipeline_layout(GraphicsDevice const &gpu) {
		vk::PipelineLayoutCreateInfo pipeline_layout_create_info{
			.setLayoutCount = 0,
			.pushConstantRangeCount = 0,
		};
		return {gpu.device, pipeline_layout_create_info};
	}

	vk::raii::SwapchainKHR GraphicsState::create_swapchain(
		vk::raii::SurfaceKHR const &surface,
		GraphicsDevice const &gpu,
		SurfaceInfo const &surface_info,
		vk::raii::SwapchainKHR const &old_swapchain
	) {
		std::array queue_family_index_array = {
			gpu.queue_family_indices.graphics,
			gpu.queue_family_indices.present,
		};
		vk::SwapchainCreateInfoKHR swapchain_create_info{
			.surface = *surface,
			.minImageCount = surface_info.surface_capabilities.maxImageCount == 0
			                 ? surface_info.surface_capabilities.minImageCount + 1
			                 : std::min(surface_info.surface_capabilities.minImageCount + 1,
			                            surface_info.surface_capabilities.maxImageCount),
			.imageFormat = surface_info.surface_format.format,
			.imageColorSpace = surface_info.surface_format.colorSpace,
			.imageExtent = surface_info.extent,
			.imageArrayLayers = 1,
			.imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
			.imageSharingMode = gpu.queue_family_indices.graphics == gpu.queue_family_indices.present ?
			                    vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent,
			.queueFamilyIndexCount = queue_family_index_array.size(),
			.pQueueFamilyIndices = queue_family_index_array.data(),
			.presentMode = surface_info.present_mode,
			.clipped = true,
			.oldSwapchain = *old_swapchain,
		};
		return {gpu.device, swapchain_create_info};
	}

	vk::raii::RenderPass GraphicsState::create_render_pass(
		GraphicsDevice const &gpu,
		SurfaceInfo const &surface_info
	) {
		vk::AttachmentDescription attachment_description{
			.format = surface_info.surface_format.format,
			.samples = vk::SampleCountFlagBits::e1,
			.loadOp = vk::AttachmentLoadOp::eDontCare,
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
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
			.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
			.srcAccessMask = {},
			.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
		}; // this forms a dependency graph
		vk::RenderPassCreateInfo render_pass_create_info{
			.attachmentCount = 1,
			.pAttachments = &attachment_description,
			.subpassCount = 1,
			.pSubpasses = &subpass_description,
			.dependencyCount = 1,
			.pDependencies = &subpass_dependency,
		};
		return {gpu.device, render_pass_create_info};
	}

	vk::raii::Pipeline GraphicsState::create_pipeline(
		GraphicsDevice const &gpu,
		vk::raii::ShaderModule const &vertex_shader_module,
		vk::raii::ShaderModule const &fragment_shader_module,
		vk::raii::PipelineLayout const &pipeline_layout,
		SurfaceInfo const &surface_info,
		vk::raii::RenderPass const &render_pass
	) {
		vk::PipelineShaderStageCreateInfo vertex_shader_stage_create_info{
			.stage = vk::ShaderStageFlagBits::eVertex,
			.module = *vertex_shader_module,
			.pName = "main",
		};
		vk::PipelineShaderStageCreateInfo fragment_shader_stage_info{
			.stage = vk::ShaderStageFlagBits::eFragment,
			.module = *fragment_shader_module,
			.pName = "main",
		};
		std::array pipeline_shader_stage_create_infos{vertex_shader_stage_create_info, fragment_shader_stage_info};
		vk::PipelineVertexInputStateCreateInfo pipeline_vertex_input_state_create_info{
			.vertexBindingDescriptionCount = 1,
			.pVertexBindingDescriptions = &Vertex::binding_description,
			.vertexAttributeDescriptionCount = Vertex::attribute_descriptions.size(),
			.pVertexAttributeDescriptions = Vertex::attribute_descriptions.data(),
		};
		vk::PipelineInputAssemblyStateCreateInfo pipeline_input_assembly_state_create_info{
			.topology = vk::PrimitiveTopology::eTriangleStrip,
			.primitiveRestartEnable = false,
		};
		vk::Extent2D const &swapchain_extent = surface_info.extent;
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
			.polygonMode = vk::PolygonMode::eFill, // eFill eLine ePoint
//		.cullMode = vk::CullModeFlagBits::eNone,
//		.frontFace = vk::FrontFace::eClockwise,
//		.depthBiasEnable = false,
			.lineWidth = 4.0f,
		};
		vk::PipelineMultisampleStateCreateInfo pipeline_multisample_state_create_info{
			.rasterizationSamples = vk::SampleCountFlagBits::e1,
			.sampleShadingEnable = false,
		};
		vk::PipelineColorBlendAttachmentState pipeline_color_blend_attachment_state{
			.blendEnable = false,
			.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
			                  vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
		};
		vk::PipelineColorBlendStateCreateInfo pipeline_color_blend_state_create_info{
			.attachmentCount = 1,
			.pAttachments = &pipeline_color_blend_attachment_state,
		};


		vk::GraphicsPipelineCreateInfo graphics_pipeline_create_info{
			.stageCount = pipeline_shader_stage_create_infos.size(),
			.pStages = pipeline_shader_stage_create_infos.data(),
			.pVertexInputState = &pipeline_vertex_input_state_create_info,
			.pInputAssemblyState = &pipeline_input_assembly_state_create_info,
			.pViewportState = &pipeline_viewport_state_create_info,
			.pRasterizationState = &pipeline_rasterization_state_create_info,
			.pMultisampleState = &pipeline_multisample_state_create_info,
			.pColorBlendState = &pipeline_color_blend_state_create_info,
			.layout = *pipeline_layout,
			.renderPass = *render_pass,
			.subpass = 0,
		};
		return {gpu.device, nullptr, graphics_pipeline_create_info};
	}
} // av
