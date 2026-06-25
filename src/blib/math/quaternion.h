#pragma once

#include <blib/math/vector.h>
#include <blib/math/angle.h>
#include <blib/math/trigonometry.h>

namespace blib
{
    namespace math
    {
        template<class Type>
        class Quatenion
        {
        public:
            Type w, x, y, z;

            Quatenion()
            {
                ::memset(this, 0, sizeof(Quatenion));
            }

            Quatenion(const Type& a_w, const Type& a_x, const Type& a_y, const Type& a_z)
            {
                this->w = a_w;
                this->x = a_x;
                this->y = a_y;
                this->z = a_z;
            }

            Quatenion(const Type& a_w, const Vector<Type, 3>& axis)
            {
                this->w = a_w;
                this->x = axis.data[0];
                this->y = axis.data[1];
                this->z = axis.data[2];
            }

            Quatenion(const blib::math::AngleRadian<Type>& angle, const blib::math::Vector<Type, 3>& axis)
            {
                const Type halfAngle = angle.data * static_cast<Type>(0.5);
                const Type sinHalf = blib::math::sin(halfAngle);
                const Type cosHalf = blib::math::cos(halfAngle);

                *this = Quatenion(cosHalf, axis * sinHalf);
            }

            Quatenion(const blib::math::AngleDegree<Type>& angle, const blib::math::Vector<Type, 3>& axis)
            {
                *this = Quatenion(angle.toRadian(), axis);
            }

            Quatenion normalize() const
            {
                Type len = blib::math::sqrt(this->w * this->w + this->x * this->x + this->y * this->y + this->z * this->z);
                return Quatenion(this->w / len, this->x / len, this->y / len, this->z / len);
            }

            Quatenion conjugate() const
            {
                return Quatenion(this->w, -this->x, -this->y, -this->z);
            }

            Quatenion inverse() const
            {
                Type normSq = this->w * this->w + this->x * this->x + this->y * this->y + this->z * this->z;
                Quatenion conj = this->conjugate();
                return Quatenion(conj.w / normSq, conj.x / normSq, conj.y / normSq, conj.z / normSq);
            }

        };

        template<class Type>
        Vector<Type, 3> rotate(const Vector<Type, 3>& lhs, const Quatenion<Type>& q)
        {
            Quatenion<Type> qn = q.normalize();
            Quatenion<Type> p(static_cast<Type>(0), lhs.data[0], lhs.data[1], lhs.data[2]);
            Quatenion<Type> qp = ::operator*(qn, p);
            Quatenion<Type> result = ::operator*(qp, qn.conjugate());
            return Vector<Type, 3>(result.x, result.y, result.z);
        }
    }
}

template<class Type>
blib::math::Quatenion<Type> operator*(const blib::math::Quatenion<Type>& lhs, const blib::math::Quatenion<Type>& rhs)
{
    return blib::math::Quatenion<Type>(
        lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z,
        lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x,
        lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w
    );
}
