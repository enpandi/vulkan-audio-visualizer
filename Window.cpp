#include "Window.hpp"

#include "constants.hpp"
#include <iostream>
#include <utility>

av::Window::Window(bool &framebuffer_resized)
	: Window{framebuffer_resized, vkfw::initUnique()} {}

av::Window::Window(bool &framebuffer_resized, vkfw::UniqueInstance &&instance) :
	vkfw::UniqueWindow{vkfw::createWindowUnique(
		av::constants::WIDTH, av::constants::HEIGHT, av::constants::TITLE,
		{.floating = av::constants::ON_TOP}
	)}
	, instance{std::move(instance)} {
	vkfw::setErrorCallback([](int error_code, char const *const description) {
		std::cerr << "glfw error callback: error code " << error_code << ", " << description << std::endl;
	});
	(*this)->callbacks()->on_framebuffer_resize = [&](vkfw::Window const &, size_t, size_t) {
		framebuffer_resized = true;
	};
}
