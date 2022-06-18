#include "Framebuffer.hpp"

#include <array>
#include <utility>

namespace av {
	Framebuffer::Framebuffer(
		vk::raii::ImageView &&image_view,
		vk::raii::Framebuffer &&framebuffer
	)
		: image_view{std::move(image_view)}
		, _framebuffer{std::move(framebuffer)} {}

	Framebuffers::Framebuffers(
		GraphicsDevice const &gpu,
		SurfaceInfo const &surface_info,
		vk::raii::SwapchainKHR const &swapchain,
		vk::raii::RenderPass const &render_pass
	) {
		auto images = swapchain.getImages();
		this->reserve(images.size());
		for (auto &&image : images) {
			vk::ImageViewCreateInfo image_view_create_info{
				.image = image,
				.viewType = vk::ImageViewType::e2D,
				.format = surface_info.surface_format.format,
				.components{
//				.r = vk::ComponentSwizzle::eIdentity,
//				.g = vk::ComponentSwizzle::eIdentity,
//				.b = vk::ComponentSwizzle::eIdentity,
//				.a = vk::ComponentSwizzle::eIdentity,
				},
				.subresourceRange{
					.aspectMask = vk::ImageAspectFlagBits::eColor,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1,
				},
			};
			vk::raii::ImageView image_view{gpu.device, image_view_create_info};
			std::array attachments{*image_view};
			vk::FramebufferCreateInfo framebuffer_create_info{
				.renderPass = *render_pass,
				.attachmentCount = attachments.size(),
				.pAttachments = attachments.data(),
				.width = surface_info.extent.width,
				.height = surface_info.extent.height,
				.layers = 1,
			};
			vk::raii::Framebuffer framebuffer{gpu.device, framebuffer_create_info};
			// https://stackoverflow.com/a/17011117
			this->emplace_back(std::move(image_view), std::move(framebuffer));
		}
	}
} // av
