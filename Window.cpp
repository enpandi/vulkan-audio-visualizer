#include "Window.hpp"

#include "constants.hpp"
#include <iostream>
#include <utility>

namespace av {
	Window::Window(bool &framebuffer_resized) :
		vkfw::UniqueWindow{vkfw::createWindowUnique(
			constants::WIDTH, constants::HEIGHT, constants::TITLE,
			{.floating = constants::ON_TOP}
		)} {
		vkfw::setErrorCallback([](int error_code, char const *const description) {
			std::cerr << "glfw error callback: error code " << error_code << ", " << description << std::endl;
		});
		(*this)->callbacks()->on_framebuffer_resize = [&](vkfw::Window const &, size_t, size_t) {
			framebuffer_resized = true;
		};
	}
} // av
