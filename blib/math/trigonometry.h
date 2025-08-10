#pragma once

namespace blib
{
    namespace math
    {
        template<class Type>
        Type sin(const Type& arg);

        template<class Type>
        Type cos(const Type& arg);

        template<class Type>
        void sincos(const Type& arg, Type& resSin, Type& resCos);
    }
}

#include <blib/math/impl/trigonometrycommonimpl.inl>
