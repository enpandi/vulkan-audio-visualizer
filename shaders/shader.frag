#version 410

layout(location = 0) in vec3 fracColor;
layout(location = 0) out vec4 outColor; // location=0 depends on render pass index

void main() {
    outColor = vec4(fracColor, 1.0);
}
