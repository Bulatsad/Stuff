#pragma once

#include <blib/core/math/consts.h>
#include <blib/core/math/utilfuncs.h>

namespace blib
{
    namespace math
    {
        template<class Type>
        class AngleDegree;
        template<class Type>
        class AngleRadian;

        template<class Type>
        class AngleDegree
        {
        public:
            Type data;

            AngleDegree()
            {
                this->data = Type();
            }
            AngleDegree(const Type& angle)
            {
                this->data = blib::math::fmod(angle, static_cast<Type>(360));
                if (this->data < static_cast<Type>(0))
                    this->data += static_cast<Type>(360);
            }
            AngleRadian<Type> toRadian() const
            {
                return AngleRadian<Type>(this->data * blib::math::piDiv180);
            }

        };

        template<class Type>
        class AngleRadian
        {
        public:
            Type data;

            AngleRadian()
            {
                this->data = Type();
            }
            AngleRadian(const Type& angle)
            {
                this->data = blib::math::fmod(angle, static_cast<Type>(2 * blib::math::pi));
                if (this->data < static_cast<Type>(0))
                    this->data += static_cast<Type>(2 * blib::math::pi);
            }
            AngleDegree<Type> toDergee() const
            {
                return AngleDegree<Type>(this->data * blib::math::c180DivPi);
            }
        };

        typedef AngleDegree<float> AngleDegreef;
        typedef AngleRadian<float> AngleRadianf;
    }

}

template<class Type>
blib::math::AngleDegree<Type> operator+(const blib::math::AngleDegree<Type>& lhs, const blib::math::AngleDegree<Type>& rhs)
{
    return blib::math::AngleDegree<Type>(lhs.data + rhs.data);
}

template<class Type>
blib::math::AngleDegree<Type> operator-(const blib::math::AngleDegree<Type>& lhs, const blib::math::AngleDegree<Type>& rhs)
{
    return blib::math::AngleDegree<Type>(lhs.data - rhs.data);
}

template<class Type>
blib::math::AngleDegree<Type> operator*(const blib::math::AngleDegree<Type>& lhs, const blib::math::AngleDegree<Type>& rhs)
{
    return blib::math::AngleDegree<Type>(lhs.data * rhs.data);
}

template<class Type>
blib::math::AngleDegree<Type> operator*(const blib::math::AngleDegree<Type>& lhs, const Type& rhs)
{
    return blib::math::AngleDegree<Type>(lhs.data * rhs);
}


template<class Type>
blib::math::AngleRadian<Type> operator+(const blib::math::AngleRadian<Type>& lhs, const blib::math::AngleRadian<Type>& rhs)
{
    return blib::math::AngleRadian<Type>(lhs.data + rhs.data);
}

template<class Type>
blib::math::AngleRadian<Type> operator+(const blib::math::AngleRadian<Type>& lhs, const Type& rhs)
{
    return blib::math::AngleRadian<Type>(lhs.data + rhs);
}

