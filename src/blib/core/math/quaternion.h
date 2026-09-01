#pragma once

#include <blib/core/math/vector.h>
#include <blib/core/math/angle.h>
#include <blib/core/math/trigonometry.h>

#include <cstring>

#ifdef COMPILE_ASSIMP_COMPATIBLE
#include <assimp/quaternion.h>
#endif // COMPILE_ASSIMP_COMPATIBLE

// Forward declaration оператора умножения кватернионов: он объявлен
// в глобальном неймспейсе ниже по файлу, но нужен уже в blib::math::rotate
// (GCC требует, чтобы qualified-id ::operator* был виден в точке
// определения шаблона — двухфазный поиск).
namespace blib { namespace math { template<class Type> class Quaternion; } }

template<class Type>
blib::math::Quaternion<Type> operator*(const blib::math::Quaternion<Type>& lhs, const blib::math::Quaternion<Type>& rhs);

namespace blib
{
    namespace math
    {
        template<class Type>
        class Quaternion
        {
        public:
            Type w, x, y, z;

            Quaternion()
            {
                ::memset(this, 0, sizeof(Quaternion));
            }

            Quaternion(const Type& a_w, const Type& a_x, const Type& a_y, const Type& a_z)
            {
                this->w = a_w;
                this->x = a_x;
                this->y = a_y;
                this->z = a_z;
            }

            Quaternion(const Type& a_w, const Vector<Type, 3>& axis)
            {
                this->w = a_w;
                this->x = axis.data[0];
                this->y = axis.data[1];
                this->z = axis.data[2];
            }

            Quaternion(const blib::math::AngleRadian<Type>& angle, const blib::math::Vector<Type, 3>& axis)
            {
                const Type halfAngle = angle.data * static_cast<Type>(0.5);
                const Type sinHalf = blib::math::sin(halfAngle);
                const Type cosHalf = blib::math::cos(halfAngle);

                *this = Quaternion(cosHalf, axis * sinHalf);
            }

            Quaternion(const blib::math::AngleDegree<Type>& angle, const blib::math::Vector<Type, 3>& axis)
            {
                *this = Quaternion(angle.toRadian(), axis);
            }

            template<class OtherType>
            Quaternion(const Quaternion<OtherType>& rhs)
            {
                this->w = static_cast<Type>(rhs.w);
                this->x = static_cast<Type>(rhs.x);
                this->y = static_cast<Type>(rhs.y);
                this->z = static_cast<Type>(rhs.z);
            }

#ifdef COMPILE_ASSIMP_COMPATIBLE
            bool loadFromAssimp(const aiQuaternion* paiquaternion)
            {
                this->x = paiquaternion->x;
                this->y = paiquaternion->y;
                this->z = paiquaternion->z;
                this->w = paiquaternion->w;

                return true;
            }
#endif // COMPILE_ASSIMP_COMPATIBLE

            Quaternion normalize() const
            {
                Type len = blib::math::sqrt(this->w * this->w + this->x * this->x + this->y * this->y + this->z * this->z);
                return Quaternion(this->w / len, this->x / len, this->y / len, this->z / len);
            }

            Quaternion conjugate() const
            {
                return Quaternion(this->w, -this->x, -this->y, -this->z);
            }

            Quaternion inverse() const
            {
                Type normSq = this->w * this->w + this->x * this->x + this->y * this->y + this->z * this->z;
                Quaternion conj = this->conjugate();
                return Quaternion(conj.w / normSq, conj.x / normSq, conj.y / normSq, conj.z / normSq);
            }

        };

        template<class Type>
        Vector<Type, 3> rotate(const Vector<Type, 3>& lhs, const Quaternion<Type>& q)
        {
            Quaternion<Type> qn = q.normalize();
            Quaternion<Type> p(static_cast<Type>(0), lhs.data[0], lhs.data[1], lhs.data[2]);
            Quaternion<Type> qp = ::operator*(qn, p);
            Quaternion<Type> result = ::operator*(qp, qn.conjugate());
            return Vector<Type, 3>(result.x, result.y, result.z);
        }

        template<class Type>
        Type dot(const Quaternion<Type>& lhs, const Quaternion<Type>& rhs)
        {
            return lhs.w * rhs.w + lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
        }

        template<class Type>
        Quaternion<Type> nlerp(const Quaternion<Type>& lhs, const Quaternion<Type>& rhs, Type t)
        {
            Type sign = blib::math::dot(lhs, rhs) < Type(0) ? Type(-1) : Type(1);
            return Quaternion<Type>(
                lhs.w * (Type(1) - t) + sign * rhs.w * t,
                lhs.x * (Type(1) - t) + sign * rhs.x * t,
                lhs.y * (Type(1) - t) + sign * rhs.y * t,
                lhs.z * (Type(1) - t) + sign * rhs.z * t
            ).normalize();
        }
    }
}

template<class Type>
blib::math::Quaternion<Type> operator*(const blib::math::Quaternion<Type>& lhs, const blib::math::Quaternion<Type>& rhs)
{
    return blib::math::Quaternion<Type>(
        lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z,
        lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x,
        lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w
    );
}
