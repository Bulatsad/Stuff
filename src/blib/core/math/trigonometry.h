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
    
        template<class Type>
        Type tan(const Type& arg);
    }
}

#include <blib/core/math/impl/trigonometrycommonimpl.inl>
