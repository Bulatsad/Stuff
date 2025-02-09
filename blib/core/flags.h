#pragma once

#include <type_traits>

namespace blib
{
    namespace core
    {
        template<class EnunFlags, class StoteType = std::underlying_type_t<EnunFlags> >
        struct Flags
        {
            StoteType storage;
            bool isUp(EnunFlags flag)
            {
                return this->storage && flag;
            }

            bool isDown(EnunFlags flag)
            {
                return !this->isUp(flag);
            }
        };
    }
}
