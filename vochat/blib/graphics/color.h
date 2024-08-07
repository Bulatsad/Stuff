#pragma once

#include <blib/config.h>
#include <stdint.h>

namespace blib
{
    namespace graphics
    {
        class __blib_api Color
        {
        public:
            uint8_t red;
            uint8_t green;
            uint8_t blue;
            uint8_t alpha;

            Color();
            Color(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha);

            static const Color Black; 
            static const Color BlackAlpha;
        };
    }
}
