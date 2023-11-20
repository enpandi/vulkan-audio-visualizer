#ifndef AUDIO_VISUALIZER_VERTEX_HPP
#define AUDIO_VISUALIZER_VERTEX_HPP

#include "graphics_headers.hpp"
#include <array>

namespace av {
	struct Vertex {
		struct Position {
			float x, y;
		} position;
		struct Color {
			float r, g, b;
		} color;
		float color_multiplier;

		static vk::VertexInputBindingDescription const binding_description;
		static std::array<vk::VertexInputAttributeDescription, 3> const attribute_descriptions;
	};

	constexpr vk::VertexInputBindingDescription Vertex::binding_description{
		.binding = 0,
		.stride = sizeof(Vertex),
		.inputRate = vk::VertexInputRate::eVertex,
	};

	constexpr std::array<vk::VertexInputAttributeDescription, 3> Vertex::attribute_descriptions{
		vk::VertexInputAttributeDescription{
			.location = 0,
			.binding = 0,
			.format = vk::Format::eR32G32Sfloat,
			.offset = offsetof(Vertex, position),
		},
		vk::VertexInputAttributeDescription{
			.location = 1,
			.binding = 0,
			.format = vk::Format::eR32G32B32Sfloat,
			.offset = offsetof(Vertex, color),
		},
		vk::VertexInputAttributeDescription{
			.location = 2,
			.binding = 0,
			.format = vk::Format::eR32Sfloat,
			.offset = offsetof(Vertex, color_multiplier),
		},
	};
} // av

#endif //AUDIO_VISUALIZER_VERTEX_HPP
