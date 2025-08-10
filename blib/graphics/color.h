#pragma once

#include <blib/config.h>
#include <stdint.h>

#include <vector>

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
            static const Color White;
            static const Color Red;

        };

        typedef std::vector<Color> Colors;
        std::vector<float>makeFloatData(const Colors& colors);

    }
}
