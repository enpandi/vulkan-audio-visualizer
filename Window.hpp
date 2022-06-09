#ifndef AUDIO_VISUALIZER_WINDOW_HPP
#define AUDIO_VISUALIZER_WINDOW_HPP

#include "graphics_headers.hpp"

namespace av {
	struct Window : vkfw::UniqueWindow {
	public:
		explicit Window(bool &framebuffer_resized);
	private:
		vkfw::UniqueInstance instance; // questionable
	};
}


#endif //AUDIO_VISUALIZER_WINDOW_HPP
