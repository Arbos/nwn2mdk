#pragma once

template <typename T>
class Vector3 {
public:
	T x, y, z;

	Vector3(T x, T y, T z)
	{
		this->x = x;
		this->y = y;
		this->z = z;
	}

	Vector3() : Vector3(0, 0, 0)
	{
	}

	T &operator[](unsigned i)
	{
		return (&x)[i];
	}

	bool operator==(const Vector3 &v) const
	{
		return x == v.x && y == v.y && z == v.z;
	}
};

static_assert(sizeof(Vector3<float>) == 12);

template <typename T>
class Vector4 {
public:
	T x, y, z, w;

	Vector4() {}

	Vector4(T x, T y, T z, T w)
	{
		this->x = x;
		this->y = y;
		this->z = z;
		this->w = w;
	}

	T &operator[](unsigned i)
	{
		return (&x)[i];
	}
};
