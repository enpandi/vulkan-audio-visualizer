//
// Created by panda on 22/06/11.
//

#ifndef AUDIO_VISUALIZER_ALLOCATOR_HPP
#define AUDIO_VISUALIZER_ALLOCATOR_HPP

#include "GraphicsDevice.hpp"
#include "graphics_headers.hpp"

namespace av {
	class Allocator : public vma::Allocator {
	public:
		Allocator(vk::raii::Instance const &, GraphicsDevice const &);

		~Allocator();

		Allocator(Allocator const &) = delete;

		Allocator &operator=(Allocator const &) = delete;

	private:
		static vma::Allocator create_allocator(vk::raii::Instance const &, GraphicsDevice const &);
	};
} // av

#endif //AUDIO_VISUALIZER_ALLOCATOR_HPP
