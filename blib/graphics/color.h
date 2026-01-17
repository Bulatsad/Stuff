#pragma once

#include <blib/config.h>
#include <blib/blibint.h>

#include <vector>

namespace blib
{
    namespace graphics
    {
        class __blib_api Color
        {
        public:
            buint8 red;
            buint8 green;
            buint8 blue;
            buint8 alpha;

            Color();
            Color(buint8 red, buint8 green, buint8 blue, buint8 alpha);
            
            static const Color Black; 
            static const Color BlackAlpha;
            static const Color White;
            static const Color Red;
            static const Color Transparent;

            static buint8 bytesPerPixel() { return 4; }
        };

        typedef std::vector<Color> Colors;
        std::vector<float>makeFloatData(const Colors& colors);

    }
}
