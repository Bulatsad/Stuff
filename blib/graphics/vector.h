#pragma once

#include <blib/inline.h>

namespace blib
{
    namespace graphics
    {
        template <typename T>
        class Vector2
        {
        public:

            Vector2();
            Vector2(T X, T Y);

            template <typename U>
            explicit Vector2(const Vector2<U>& vector);

            T x;
            T y;
        };

        template <typename T>
        class Vector3
        {
        public:

            Vector3();
            Vector3(T X, T Y, T Z);

            template <typename U>
            explicit Vector3(const Vector3<U>& vector);

            T x;
            T y;
            T z;
        };

        template <typename T>
        class Vector4
        {
        public:

            Vector4();
            Vector4(T X, T Y, T Z, T W);

            template <typename U>
            explicit Vector4(const Vector4<U>& vector);

            T x;
            T y;
            T z;
            T w;

            T& r = x;
            T& g = y;
            T& b = z;
            T& a = w;
        };

        typedef Vector2<float> Vector2f;
        typedef Vector2<int> Vector2i;

        typedef Vector3<float> Vector3f;
        typedef Vector3<int> Vector3i;
        typedef Vector3f Vertex;

        typedef Vector4<float> Vector4f;
    }
}

template <typename T>
blib::graphics::Vector2<T> operator-(const blib::graphics::Vector2<T>& right);

template <typename T>
blib::graphics::Vector2<T>& operator+=(blib::graphics::Vector2<T>& left, const blib::graphics::Vector2<T>& right);

template <typename T>
blib::graphics::Vector2<T>& operator-=(blib::graphics::Vector2<T>& left, const blib::graphics::Vector2<T>& right);

template <typename T>
blib::graphics::Vector2<T> operator+(const blib::graphics::Vector2<T>& left, const blib::graphics::Vector2<T>& right);

template <typename T>
blib::graphics::Vector2<T> operator-(const blib::graphics::Vector2<T>& left, const blib::graphics::Vector2<T>& right);

template <typename T>
blib::graphics::Vector2<T> operator*(const blib::graphics::Vector2<T>& left, T right);

template <typename T>
blib::graphics::Vector2<T> operator*(T left, const blib::graphics::Vector2<T>& right);

template <typename T>
blib::graphics::Vector2<T>& operator*=(blib::graphics::Vector2<T>& left, T right);

template <typename T>
blib::graphics::Vector2<T> operator/(const blib::graphics::Vector2<T>& left, T right);

template <typename T>
blib::graphics::Vector2<T>& operator/=(blib::graphics::Vector2<T>& left, T right);

template <typename T>
bool operator ==(const blib::graphics::Vector2<T>& left, const blib::graphics::Vector2<T>& right);

template <typename T>
bool operator !=(const blib::graphics::Vector2<T>& left, const blib::graphics::Vector2<T>& right);

template <typename T>
__blib_inline blib::graphics::Vector2<T>::Vector2() 
{   
    this->x = 0;
    this->y = 0;
}

template <typename T>
__blib_inline blib::graphics::Vector2<T>::Vector2(T X, T Y) 
{
    this->x = X;
    this->y = Y;
}

template <typename T>
template <typename U>
__blib_inline blib::graphics::Vector2<T>::Vector2(const Vector2<U>& vector) 
{
    this->x = static_cast<T>(vector.x)
    this->y = static_cast<T>(vector.y)

}

template <typename T>
__blib_inline blib::graphics::Vector2<T> operator -(const blib::graphics::Vector2<T>& right)
{
    return Vector2<T>(-right.x, -right.y);
}

template <typename T>
__blib_inline blib::graphics::Vector2<T>& operator +=(blib::graphics::Vector2<T>& left, const blib::graphics::Vector2<T>& right)
{
    left.x += right.x;
    left.y += right.y;

    return left;
}

template <typename T>
__blib_inline blib::graphics::Vector2<T>& operator-=(blib::graphics::Vector2<T>& left, const blib::graphics::Vector2<T>& right)
{
    left.x -= right.x;
    left.y -= right.y;

    return left;
}

template <typename T>
__blib_inline blib::graphics::Vector2<T> operator+(const blib::graphics::Vector2<T>& left, const blib::graphics::Vector2<T>& right)
{
    return Vector2<T>(left.x + right.x, left.y + right.y);
}

template <typename T>
__blib_inline blib::graphics::Vector2<T> operator-(const blib::graphics::Vector2<T>& left, const blib::graphics::Vector2<T>& right)
{
    return Vector2<T>(left.x - right.x, left.y - right.y);
}

template <typename T>
__blib_inline blib::graphics::Vector2<T> operator*(const blib::graphics::Vector2<T>& left, T right)
{
    return Vector2<T>(left.x * right, left.y * right);
}

template <typename T>
__blib_inline blib::graphics::Vector2<T> operator*(T left, const blib::graphics::Vector2<T>& right)
{
    return Vector2<T>(right.x * left, right.y * left);
}

template <typename T>
__blib_inline blib::graphics::Vector2<T>& operator*=(blib::graphics::Vector2<T>& left, T right)
{
    left.x *= right;
    left.y *= right;

    return left;
}

template <typename T>
__blib_inline blib::graphics::Vector2<T> operator/(const blib::graphics::Vector2<T>& left, T right)
{
    return Vector2<T>(left.x / right, left.y / right);
}

template <typename T>
__blib_inline blib::graphics::Vector2<T>& operator/=(blib::graphics::Vector2<T>& left, T right)
{
    left.x /= right;
    left.y /= right;

    return left;
}

template <typename T>
__blib_inline bool operator==(const blib::graphics::Vector2<T>& left, const blib::graphics::Vector2<T>& right)
{
    return (left.x == right.x) && (left.y == right.y);
}

template <typename T>
__blib_inline bool operator!=(const blib::graphics::Vector2<T>& left, const blib::graphics::Vector2<T>& right)
{
    return (left.x != right.x) || (left.y != right.y);
}

template <typename T>
blib::graphics::Vector3<T> operator-(const blib::graphics::Vector3<T>& left);

template <typename T>
blib::graphics::Vector3<T>& operator+=(blib::graphics::Vector3<T>& left, const blib::graphics::Vector3<T>& right);

template <typename T>
blib::graphics::Vector3<T>& operator-=(blib::graphics::Vector3<T>& left, const blib::graphics::Vector3<T>& right);

template <typename T>
blib::graphics::Vector3<T> operator+(const blib::graphics::Vector3<T>& left, const blib::graphics::Vector3<T>& right);

template <typename T>
blib::graphics::Vector3<T> operator-(const blib::graphics::Vector3<T>& left, const blib::graphics::Vector3<T>& right);

template <typename T>
blib::graphics::Vector3<T> operator*(const blib::graphics::Vector3<T>& left, T right);

template <typename T>
blib::graphics::Vector3<T> operator*(T left, const blib::graphics::Vector3<T>& right);

template <typename T>
blib::graphics::Vector3<T>& operator*=(blib::graphics::Vector3<T>& left, T right);

template <typename T>
blib::graphics::Vector3<T> operator/(const blib::graphics::Vector3<T>& left, T right);

template <typename T>
blib::graphics::Vector3<T>& operator/=(blib::graphics::Vector3<T>& left, T right);

template <typename T>
bool operator ==(const blib::graphics::Vector3<T>& left, const blib::graphics::Vector3<T>& right);

template <typename T>
bool operator !=(const blib::graphics::Vector3<T>& left, const blib::graphics::Vector3<T>& right);

template <typename T>
__blib_inline blib::graphics::Vector3<T>::Vector3() 
{
    this->x = 0;
    this->y = 0;
    this->z = 0;
}

template <typename T>
__blib_inline blib::graphics::Vector3<T>::Vector3(T X, T Y, T Z)
{
    this->x = X;
    this->y = Y;
    this->z = Z;
}

template <typename T>
template <typename U>
__blib_inline blib::graphics::Vector3<T>::Vector3(const Vector3<U>& vector)
{
    this->x = x(static_cast<T>(vector.x));
    this->y = y(static_cast<T>(vector.y));
    this->z = z(static_cast<T>(vector.z));
}

template <typename T>
__blib_inline blib::graphics::Vector3<T> operator-(const blib::graphics::Vector3<T>& left)
{
    return Vector3<T>(-left.x, -left.y, -left.z);
}

template <typename T>
__blib_inline blib::graphics::Vector3<T>& operator+=(blib::graphics::Vector3<T>& left, const blib::graphics::Vector3<T>& right)
{
    left.x += right.x;
    left.y += right.y;
    left.z += right.z;

    return left;
}

template <typename T>
__blib_inline blib::graphics::Vector3<T>& operator-=(blib::graphics::Vector3<T>& left, const blib::graphics::Vector3<T>& right)
{
    left.x -= right.x;
    left.y -= right.y;
    left.z -= right.z;

    return left;
}

template <typename T>
__blib_inline blib::graphics::Vector3<T> operator+(const blib::graphics::Vector3<T>& left, const blib::graphics::Vector3<T>& right)
{
    return Vector3<T>(left.x + right.x, left.y + right.y, left.z + right.z);
}

template <typename T>
__blib_inline blib::graphics::Vector3<T> operator-(const blib::graphics::Vector3<T>& left, const blib::graphics::Vector3<T>& right)
{
    return Vector3<T>(left.x - right.x, left.y - right.y, left.z - right.z);
}

template <typename T>
__blib_inline blib::graphics::Vector3<T> operator*(const blib::graphics::Vector3<T>& left, T right)
{
    return Vector3<T>(left.x * right, left.y * right, left.z * right);
}

template <typename T>
__blib_inline blib::graphics::Vector3<T> operator*(T left, const blib::graphics::Vector3<T>& right)
{
    return Vector3<T>(right.x * left, right.y * left, right.z * left);
}

template <typename T>
__blib_inline blib::graphics::Vector3<T>& operator*=(blib::graphics::Vector3<T>& left, T right)
{
    left.x *= right;
    left.y *= right;
    left.z *= right;

    return left;
}

template <typename T>
__blib_inline blib::graphics::Vector3<T> operator/(const blib::graphics::Vector3<T>& left, T right)
{
    return Vector3<T>(left.x / right, left.y / right, left.z / right);
}

template <typename T>
__blib_inline blib::graphics::Vector3<T>& operator/=(blib::graphics::Vector3<T>& left, T right)
{
    left.x /= right;
    left.y /= right;
    left.z /= right;

    return left;
}

template <typename T>
__blib_inline bool operator ==(const blib::graphics::Vector3<T>& left, const blib::graphics::Vector3<T>& right)
{
    return (left.x == right.x) && (left.y == right.y) && (left.z == right.z);
}

template <typename T>
__blib_inline bool operator !=(const blib::graphics::Vector3<T>& left, const blib::graphics::Vector3<T>& right)
{
    return (left.x != right.x) || (left.y != right.y) || (left.z != right.z);
}

template<typename T>
inline blib::graphics::Vector4<T>::Vector4(T X, T Y, T Z, T W)
{
    this->x = X;
    this->y = Y;
    this->z = Z;
    this->w = W;
}