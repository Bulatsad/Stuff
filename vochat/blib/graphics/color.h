#pragma once

#include <stdint.h>

namespace blib
{
    namespace graphics
    {
        class Color
        {
        public:
            uint8_t red;
            uint8_t green;
            uint8_t blue;
            uint8_t alpha;

            Color(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha);

            static const Color Black;
        };
    }
}
