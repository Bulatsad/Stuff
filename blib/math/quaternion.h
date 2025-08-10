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

            Quatenion(const Type& a_w, const Type& a_x, const Type& a_y, const Type& a_z)
            {
                this->w = a _w;
                this->x = a _x;
                this->y = a _y;
                this->z = a _z;
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
                const Type sin = blib::math::sin(angle.data * static_cast<Type>(0.5));

                *this = Quatenion(blib::math::cos(angle * static_cast<T>(0.5)), axis * sin);
            }

            Quatenion(const blib::math::AngleDegree<Type>& angle, const blib::math::Vector<Type, 3>& axis)
            {
                *this = Quatenion(angle.toRadian(), axis);
            }



        };
    }
}
