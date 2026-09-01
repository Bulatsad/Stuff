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
            bool isUp(EnunFlags flag) const
            {
                // Побитовая проверка (важно: именно &, не логический &&,
                // иначе любой ненулевой флаг совпадёт с любым ненулевым storage)
                return (this->storage & static_cast<StoteType>(flag)) != 0;
            }

            bool isDown(EnunFlags flag) const
            {
                return !this->isUp(flag);
            }
        };
    }
}
