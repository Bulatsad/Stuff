#pragma once

#include <blib/config.h>
#include <blib/inline.h>

#include <cmath>

namespace blib
{
    namespace math
    {
        template<class Type>
        Type fmod(const Type& x, const Type& y)
        {
            return std::fmod(x, y);
        }
    }
}
