#pragma once

#include <cmath>

namespace math {

constexpr float pi = 3.14159265358979f;

inline float radians(float degrees) {
	return degrees * pi / 180.0f;
}

// 4x4 matrix stored column by column (column-major), like GLSL mat4
struct mat4 {
	float m[16];
};

inline float& at(mat4& a, int row, int col) {
	return a.m[col * 4 + row];
}

inline float at(const mat4& a, int row, int col) {
	return a.m[col * 4 + row];
}

inline mat4 identity() {
	mat4 r = {};
	at(r, 0, 0) = 1.0f;
	at(r, 1, 1) = 1.0f;
	at(r, 2, 2) = 1.0f;
	at(r, 3, 3) = 1.0f;
	return r;
}

inline mat4 operator*(const mat4& a, const mat4& b) {
	mat4 r = {};
	for (int row = 0; row < 4; ++row) {
		for (int col = 0; col < 4; ++col) {
			for (int k = 0; k < 4; ++k) {
				at(r, row, col) += at(a, row, k) * at(b, k, col);
			}
		}
	}
	return r;
}

inline mat4 translate(float x, float y, float z) {
	mat4 r = identity();
	at(r, 0, 3) = x;
	at(r, 1, 3) = y;
	at(r, 2, 3) = z;
	return r;
}

// Rotation around X axis, angle in radians
inline mat4 rotateX(float angle) {
	const float c = std::cos(angle);
	const float s = std::sin(angle);
	mat4 r = identity();
	at(r, 1, 1) = c; at(r, 1, 2) = -s;
	at(r, 2, 1) = s; at(r, 2, 2) = c;
	return r;
}

// Rotation around Y axis, angle in radians
inline mat4 rotateY(float angle) {
	const float c = std::cos(angle);
	const float s = std::sin(angle);
	mat4 r = identity();
	at(r, 0, 0) = c;  at(r, 0, 2) = s;
	at(r, 2, 0) = -s; at(r, 2, 2) = c;
	return r;
}

// Perspective projection for Vulkan: camera looks along -Z,
// clip space Y points down and depth is mapped to [0, 1]
inline mat4 perspective(float fov_y, float aspect, float near_z, float far_z) {
	const float f = 1.0f / std::tan(fov_y / 2.0f);
	mat4 r = {};
	at(r, 0, 0) = f / aspect;
	at(r, 1, 1) = -f;
	at(r, 2, 2) = far_z / (near_z - far_z);
	at(r, 2, 3) = near_z * far_z / (near_z - far_z);
	at(r, 3, 2) = -1.0f;
	return r;
}

} // namespace math
