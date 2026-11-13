#version 450

// Must match struct Uniforms in source/application.cpp
layout(set = 0, binding = 0) uniform Uniforms {
	mat4 model;
	mat4 view_projection;
	vec3 camera_position;
	vec3 ambient_color;   float ambient_intensity;
	vec3 light_direction; float light_intensity;
	vec3 light_color;
	vec3 albedo;
	vec3 specular_color;  float shininess;
} u;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;

layout(location = 0) out vec3 v_world_position;
layout(location = 1) out vec3 v_normal;

void main() {
	// Lighting is computed in world space, so the world position goes to the fragment shader
	vec4 world = u.model * vec4(in_position, 1.0);
	gl_Position = u.view_projection * world;
	v_world_position = world.xyz;

	// Model is a pure rotation, so transpose(inverse(model)) == model
	v_normal = mat3(u.model) * in_normal;
}
