#include <cmath>

template<class Type>
Type blib::math::sin(const Type& arg)
{
    return std::sin(arg);
}

template<class Type>
Type blib::math::cos(const Type& arg)
{
    return std::cos(arg);
}

template<class Type>
void blib::math::sincos(const Type& arg, Type& resSin, Type& resCos)
{
    resSin = blib::math::sin(arg);
    resCos = blib::math::cos(arg);
}
