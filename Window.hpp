#ifndef AUDIO_VISUALIZER_WINDOW_HPP
#define AUDIO_VISUALIZER_WINDOW_HPP

#include "graphics_headers.hpp"

namespace av {
	class Window : public vkfw::UniqueWindow {
	public:
		explicit Window(bool &framebuffer_resized);
	};
} // av

#endif //AUDIO_VISUALIZER_WINDOW_HPP
