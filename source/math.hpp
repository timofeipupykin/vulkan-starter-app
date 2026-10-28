#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace math {

constexpr float pi = std::numbers::pi_v<float>;

inline float radians(float degrees) {
	return degrees * pi / 180.0f;
}

// 4x4 matrix stored column by column (column-major), like GLSL mat4
class mat4 {
public:
	static constexpr size_t dim = 4;

	float& operator()(size_t row, size_t col) {
		return m_[col * dim + row];
	}

	float operator()(size_t row, size_t col) const {
		return m_[col * dim + row];
	}

private:
	std::array<float, dim * dim> m_{};
};

inline mat4 identity() {
	mat4 tmp;
	tmp(0, 0) = 1.0f;
	tmp(1, 1) = 1.0f;
	tmp(2, 2) = 1.0f;
	tmp(3, 3) = 1.0f;
	return tmp;
}

inline mat4 operator*(const mat4& a, const mat4& b) {
	mat4 tmp;
	for (size_t row = 0; row < mat4::dim; ++row) {
		for (size_t col = 0; col < mat4::dim; ++col) {
			for (size_t k = 0; k < mat4::dim; ++k) {
				tmp(row, col) += a(row, k) * b(k, col);
			}
		}
	}
	return tmp;
}

inline mat4 translate(float x, float y, float z) {
	mat4 tmp = identity();
	tmp(0, 3) = x;
	tmp(1, 3) = y;
	tmp(2, 3) = z;
	return tmp;
}

// Rotation around X axis, angle in radians
inline mat4 rotateX(float angle) {
	const float c = std::cos(angle);
	const float s = std::sin(angle);
	mat4 tmp = identity();
	tmp(1, 1) = c; tmp(1, 2) = -s;
	tmp(2, 1) = s; tmp(2, 2) = c;
	return tmp;
}

// Rotation around Y axis, angle in radians
inline mat4 rotateY(float angle) {
	const float c = std::cos(angle);
	const float s = std::sin(angle);
	mat4 tmp = identity();
	tmp(0, 0) = c;  tmp(0, 2) = s;
	tmp(2, 0) = -s; tmp(2, 2) = c;
	return tmp;
}

// Perspective projection for Vulkan: camera looks along -Z,
// clip space Y points down and depth is mapped to [0, 1]
inline mat4 perspective(float fov_y, float aspect, float near_z, float far_z) {
	const float f = 1.0f / std::tan(fov_y / 2.0f);
	mat4 tmp;
	tmp(0, 0) = f / aspect;
	tmp(1, 1) = -f;
	tmp(2, 2) = far_z / (near_z - far_z);
	tmp(2, 3) = near_z * far_z / (near_z - far_z);
	tmp(3, 2) = -1.0f;
	return tmp;
}

} // namespace math
