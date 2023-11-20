#ifndef AUDIO_VISUALIZER_CONSTANTS_HPP
#define AUDIO_VISUALIZER_CONSTANTS_HPP

//#include <vulkan/vulkan_core.h> // is this allowed?
#include "graphics_headers.hpp"
//#include <vulkan/vulkan.h>

#include <array>
#include <string>

// todo constexpr std::string? https://stackoverflow.com/a/1563906
namespace av::constants {
	static constexpr size_t WIDTH{256};
	static constexpr size_t HEIGHT{256};
	static constexpr char const *TITLE = "av";
	static constexpr bool ON_TOP{true};

	static constexpr uint32_t VK_API_VERSION = VK_API_VERSION_1_1;
//	static constexpr auto GLOBAL_LAYERS = []() {
//		if constexpr(DEBUG) return std::array{"VK_LAYER_KHRONOS_validation"};
//		else return std::array<char const *, 0>();
//	}(); // todo make this work when DEBUG is not defined
#ifdef DEBUG
	static constexpr std::array GLOBAL_LAYERS{"VK_LAYER_KHRONOS_validation"};
#else
	static constexpr std::array<char const *, 0> GLOBAL_LAYERS;
#endif
	static constexpr std::array DEVICE_EXTENSIONS = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
	static constexpr char const *APPLICATION_NAME = "audio visualizer";
	static constexpr char const *VERTEX_SHADER_FILE_NAME = "shaders/shader.vert.spv";
	static constexpr char const *FRAGMENT_SHADER_FILE_NAME = "shaders/shader.frag.spv";

	static constexpr size_t MAX_FRAMES_IN_FLIGHT = 3;

	using index_t = uint16_t;
	static constexpr vk::IndexType vk_index_type = vk::IndexType::eUint16;
} // av

#endif //AUDIO_VISUALIZER_CONSTANTS_HPP
