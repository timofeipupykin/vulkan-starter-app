#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_color;

layout(location = 0) out vec3 v_color;

void main() {
	// Temporary: no matrices yet. Flip Y (Vulkan Y points down) and fit Z into [0, 1]
	gl_Position = vec4(in_position.x * 0.5, -in_position.y * 0.5, in_position.z * 0.25 + 0.5, 1.0);
	v_color = in_color;
}
