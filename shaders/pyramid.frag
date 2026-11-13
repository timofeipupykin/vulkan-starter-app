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

layout(location = 0) in vec3 v_world_position;
layout(location = 1) in vec3 v_normal;

layout(location = 0) out vec4 out_color;

void main() {
	// Interpolated normal is not unit length in general
	vec3 N = normalize(v_normal);
	vec3 V = normalize(u.camera_position - v_world_position);
	// Directional light: the same direction to the light for every point
	vec3 L = -u.light_direction;

	vec3 color = u.ambient_color * u.ambient_intensity * u.albedo;

	float n_dot_l = max(dot(N, L), 0.0);

	// Light comes from the front side: diffuse + Blinn-Phong specular
	if (n_dot_l > 0.0) {
		vec3 H = normalize(L + V);
		float specular = pow(max(dot(N, H), 0.0), u.shininess);

		color += (u.albedo * n_dot_l + u.specular_color * specular) *
		         u.light_color * u.light_intensity;
	}

	out_color = vec4(color, 1.0);
}
