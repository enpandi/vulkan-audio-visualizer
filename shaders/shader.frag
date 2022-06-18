#version 410

layout(location = 0) in vec3 fragColor;
layout(location = 0) out vec4 outColor; // location=0 depends on render pass index

void main() {
	outColor = vec4(fragColor, 1.0);
}
