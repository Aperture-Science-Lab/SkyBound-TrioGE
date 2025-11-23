#include "Vector3f.h"
#include <math.h>


Vector3f::Vector3f(float _x, float _y, float _z) {
	x = _x;
	y = _y;
	z = _z;
}


Vector3f Vector3f::operator+(const Vector3f& v) const {
	return Vector3f(x + v.x, y + v.y, z + v.z);
}


Vector3f Vector3f::operator-(const Vector3f& v) const {
	return Vector3f(x - v.x, y - v.y, z - v.z);
}


Vector3f Vector3f::operator*(float n) const {
	return Vector3f(x * n, y * n, z * n);
}


Vector3f Vector3f::operator/(float n) const {
	return Vector3f(x / n, y / n, z / n);
}


Vector3f Vector3f::unit() const {
	float length = sqrt(x * x + y * y + z * z);
	if (length < 0.0001f) return Vector3f(x, y, z); // Avoid division by zero
	return *this / length;
}


Vector3f Vector3f::cross(Vector3f v) const {
	return Vector3f(
		y * v.z - z * v.y,
		z * v.x - x * v.z,
		x * v.y - y * v.x
	);
}
