//
// Created by panda on 22/06/11.
//

#include "constants.hpp"
#include "Allocator.hpp"

namespace av {
	Allocator::Allocator(
		vk::raii::Instance const &instance,
		GraphicsDevice const &gpu
	)
		: vma::Allocator{create_allocator(instance, gpu)} {}

	Allocator::~Allocator() {
		destroy();
	}

	vma::Allocator Allocator::create_allocator(
		vk::raii::Instance const &instance,
		GraphicsDevice const &gpu
	) {
		vma::AllocatorCreateInfo allocator_create_info{
			.physicalDevice = *gpu.physical_device,
			.device = *gpu.device,
			.instance = *instance,
			.vulkanApiVersion = constants::VK_API_VERSION,
		};
		return vma::createAllocator(allocator_create_info);
	}
} // av
