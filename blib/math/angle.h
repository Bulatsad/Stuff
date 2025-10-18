#pragma once

#include <blib/math/consts.h>
#include <blib/math/utilfuncs.h>

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

            AngleDegree(const Type& angle)
            {
                this->data = blib::math::fmod(angle, static_cast<Type>(360));
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
           
            AngleRadian(const Type& angle)
            {
                this->data = blib::math::fmod(angle, static_cast<Type>(2 * blib::math::pi));
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
