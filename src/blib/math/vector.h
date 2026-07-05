#pragma once

#include <memory>

#include <functional>

#include <blib/math/utilfuncs.h>

#ifdef COMPILE_ASSIMP_COMPATIBLE
#include <assimp/vector3.h>
#endif // COMPILE_ASSIMP_COMPATIBLE


//namespace blib
//{
//    namespace functional
//    {
//        template <bool _Test, class _Ty = void>
//        struct create_anon_class {}; // no member "type" when !_Test
//
//        template <class _Ty>
//        struct create_anon_class<true, _Ty>
//        {
//            _Ty;
//        };
//
//    }
//}

//template<class Type>
//class Vector<Type, 3>
//{
//public:
//    union
//    {
//        Type data[3];
//        struct
//        {
//            Type x, y, z;
//        };
//    };
//};

#define blib_math_vector_template_common_code(size)                        \
    Vector& operator=(const Vector<Type, size>& rhs)                       \
    {                                                                      \
        memcpy(this->data, rhs.data, sizeof(Type) * size);                 \
        return *this;                                                      \
    }                                                                      \
    const Type& operator[](VectorSizeT index) const                        \
    {                                                                      \
        return this->data[index];                                          \
    }                                                                      \
    Type& operator[](VectorSizeT index)                                    \
    {                                                                      \
        return this->data[index];                                          \
    }                                                                      \
                                                                           \
    template<class rhsType, VectorSizeT rhsSize>                           \
    Vector(const Vector<rhsType, rhsSize>& rhs)                            \
    {                                                                      \
        if (size > rhsSize)                                                \
            throw std::runtime_error("Rhs vector size must be more or equal"); \
                                                                           \
        for (VectorSizeT i = 0; i < size; ++i)                             \
        {                                                                  \
            this->data[i] = rhs.data[i];                                   \
        }                                                                  \
    }                                                                      \
    Vector(){}


namespace blib
{
    namespace math
    {
        template<class Type>
        class Quaternion;

        typedef size_t VectorSizeT;
        template<class Type, VectorSizeT size>
        class Vector
        {
        public:
            union
            {
                Type data[size];
            };

            blib_math_vector_template_common_code(size)



            /*
            Vector(const Type& a_x, const Type& a_y, const Type& a_z)
            {
                data[0] = a_x;
                data[1] = a_y;
                data[2] = a_z;
            }
            Vector(const Type& a_x, const Type& a_y, const Type& a_z, const Type& a_w)
            {
                data[0] = a_x;
                data[1] = a_y;
                data[2] = a_z;
                data[3] = a_w;
            }
            Vector operator+(const Vector& rhs) const
            {
                Vector res = *this;
                for (size_t i = 0; i < size; ++i)
                    res.data[i] += rhs.data[i];
                return res;
            }
            Vector operator-(const Vector& rhs) const
            {
                Vector res = *this;
                for (size_t i = 0; i < size; ++i)
                    res.data[i] -= rhs.data[i];
                return res;
            }
            Vector operator+=(const Vector& rhs)
            {
                *this = *this + rhs;
            }
            Vector operator-=(const Vector& rhs)
            {
                *this = *this - rhs;
            }
            Vector operator*(const Type rhs) const
            {
                Vector res = *this;
                for (size_t i = 0; i < size; ++i)
                    res.data[i] *= rhs.data[i];
                return res;
            }
            Vector operator*(const Vector& rhs) const
            {
                Vector res = *this;
                for (size_t i = 0; i < size; ++i)
                    res.data[i] *= rhs.data[i];
                return res;
            }
            Vector operator*=(Type rhs)
            {
                *this = *this * rhs;
            }
            Vector operator/(Type rhs) const
            {
                Vector res = *this;
                for (size_t i = 0; i < size; ++i)
                    res.data[i] /= rhs.data[i];
                return res;
            }
            Vector operator/=(Type rhs)
            {
                *this = *this / rhs;
            }
            Vector operator ==(const Vector& rhs) const
            {
                for (size_t i = 0; i < size; ++i)
                {
                    if (this->data[i] != rhs.data[i])
                        return false;
                }
                return true;
            }
            Vector operator !=(const Vector& rhs) const
            {
                for (size_t i = 0; i < size; ++i)
                {
                    if (this->data[i] != rhs.data[i])
                        return true;
                }
                return false;
            }

            Vector& operator=(const Vector& rhs)
            {
                memcpy(this->data, rhs.data, sizeof(Type) * size);
                return *this;
            }

            const Type& operator[](VectorSizeT index) const
            {
                return this->data[index];
            }
            Type& operator[](VectorSizeT index)
            {
                return this->data[index];
            }
            */

            //Vector operator*(T left, const Vector& rhs);
        };

        template<class Type>
        class Vector<Type, 2>
        {
        public:
            union
            {
                Type data[2];
                struct
                {
                    Type x, y;
                };
            };

            Vector(const Type& a_x, const Type& a_y)
            {
                this->x = a_x;
                this->y = a_y;
            }

            blib_math_vector_template_common_code(2)
        };

        template<class Type>
        class Vector<Type, 3>
        {
        public:
            union
            {
                Type data[3];
                struct
                {
                    Type x, y, z;
                };
            };

            Vector(const Type& a_x, const Type& a_y, const Type& a_z)
            {
                this->x = a_x;
                this->y = a_y;
                this->z = a_z;
            }

#ifdef COMPILE_ASSIMP_COMPATIBLE
            bool loadFromAssimp(const aiVector3D* paivector3d)
            {
                this->x = paivector3d->x;
                this->y = paivector3d->y;
                this->z = paivector3d->z;

                return true;
            }
#endif // COMPILE_ASSIMP_COMPATIBLE

            blib_math_vector_template_common_code(3)
        };

        template<class Type>
        class Vector<Type, 4>
        {
        public:
            union
            {
                Type data[4];
                struct
                {
                    Type x, y, z, w;
                };
            };

            Vector(const Type& a_x, const Type& a_y, const Type& a_z, const Type& a_w)
            {
                this->x = a_x;
                this->y = a_y;
                this->z = a_z;
                this->w = a_w;
            }

            blib_math_vector_template_common_code(4)
        };

        template<class Type, VectorSizeT size>
        Vector<Type, size> operator+(const Vector<Type, size>& lhs, const Vector<Type, size>& rhs)
        {
            Vector<Type, size> res = lhs;
            for (size_t i = 0; i < size; ++i)
                res.data[i] += rhs.data[i];
            return res;
        }

        template<class Type, VectorSizeT size>
        Vector<Type, size> operator-(const Vector<Type, size>& lhs, const Vector<Type, size>& rhs)
        {
            Vector<Type, size> res = lhs;
            for (size_t i = 0; i < size; ++i)
                res.data[i] -= rhs.data[i];
            return res;
        }
        template<class Type, VectorSizeT size>
        Vector<Type, size>& operator+=(Vector<Type, size>& lhs, const Vector<Type, size>& rhs)
        {
            for (size_t i = 0; i < size; ++i)
                lhs.data[i] += rhs.data[i];
            return lhs;
        }
        template<class Type, VectorSizeT size>
        Vector<Type, size>& operator-=(Vector<Type, size>& lhs, const Vector<Type, size>& rhs)
        {
            for (size_t i = 0; i < size; ++i)
                lhs.data[i] -= rhs.data[i];
            return lhs;
        }
        template<class Type, VectorSizeT size>
        Vector<Type, size> operator*(const Vector<Type, size>& lhs, const Type rhs)
        {
            Vector<Type, size> res = lhs;
            for (size_t i = 0; i < size; ++i)
                res.data[i] *= rhs;
            return res;
        }
        template<class Type, VectorSizeT size>
        Vector<Type, size> operator*(const Vector<Type, size>& lhs, const Vector<Type, size>& rhs)
        {
            Vector<Type, size> res = lhs;
            for (size_t i = 0; i < size; ++i)
                res.data[i] *= rhs.data[i];
            return res;
        }
        template<class Type, VectorSizeT size>
        Vector<Type, size>& operator*=(Vector<Type, size>& lhs, Type rhs)
        {
            for (size_t i = 0; i < size; ++i)
                lhs.data[i] *= rhs;
            return lhs;
        }
        template<class Type, VectorSizeT size>
        Vector<Type, size> operator/(const Vector<Type, size>& lhs, Type rhs)
        {
            Vector<Type, size> res = lhs;
            for (size_t i = 0; i < size; ++i)
                res.data[i] /= rhs;
            return res;
        }
        template<class Type, VectorSizeT size>
        Vector<Type, size>& operator/=(Vector<Type, size>& lhs, Type rhs)
        {
            for (size_t i = 0; i < size; ++i)
                lhs.data[i] /= rhs;
            return lhs;
        }
        template<class Type, VectorSizeT size>
        bool operator==(const Vector<Type, size>& lhs, const Vector<Type, size>& rhs) 
        {
            for (size_t i = 0; i < size; ++i)
            {
                if (lhs.data[i] != rhs.data[i])
                    return false;
            }
            return true;
        }
        template<class Type, VectorSizeT size>
        bool operator!=(const Vector<Type, size>& lhs, const Vector<Type, size>& rhs)
        {
            for (size_t i = 0; i < size; ++i)
            {
                if (lhs.data[i] != rhs.data[i])
                    return true;
            }
            return false;
        }

        /*template<class Type, VectorSizeT size>
        inline Vector<Type, size>& Vector<Type, size>::operator=(const Vector<Type, size>& lhs, const Vector<Type, size>& rhs)
        {
            memcpy(lhs.data, rhs.data, sizeof(Type) * size);
            return lhs;
        }
        template<class Type, VectorSizeT size>
        const Type& Vector<Type, size>::operator[](const Vector<Type, size>& lhs, VectorSizeT index)
        {
            return this->data[index];
        }
        template<class Type, VectorSizeT size>
        Type& Vector<Type, size>::operator[](Vector<Type, size>& lhs, VectorSizeT index)
        {
            return this->data[index];
        }*/

        template<class Type, VectorSizeT size>
        Type magnitude(const Vector<Type, size>& vector)
        {
            float res = 0;

            for (const Type& d : vector.data)
                res += d * d;

            return blib::math::sqrt(res);
        }

        template<class Type, VectorSizeT size>
        Vector<Type, size> normalize(const Vector<Type, size>& vector)
        {
            Vector<Type, size> res;
            Type magnitudeValue = blib::math::magnitude(vector);

            for (VectorSizeT i = 0; i < size; ++i)
                res.data[i] = vector.data[i] / magnitudeValue;

            return res;
        }

        template<class Type>
        Vector<Type, 3> cross(const Vector<Type, 3>& x, const Vector<Type, 3>& y)
        {
            return Vector<Type, 3>(
                x.y * y.z - y.y * x.z,
                x.z * y.x - y.z * x.x,
                x.x * y.y - y.x * x.y
            );
        }

        template<class Type, VectorSizeT size>
        Type dot(const Vector<Type, size>& a, const Vector<Type, size>& b)
        {
            Vector<Type, size> tmp = a * b;
            Type res = 0;

            for (const Type& d : tmp.data)
                res += d;

            return res;
        }

        template<class Type, VectorSizeT size>
        Type length(const Vector<Type, size>& vector)
        {
            return blib::math::sqrt(blib::math::dot(vector, vector));
        }
    }
}


