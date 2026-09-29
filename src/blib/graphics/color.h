#pragma once

#include <blib/config.h>
#include <blib/blibint.h>

#include <vector>

namespace blib
{
    namespace graphics
    {
        class __blib_graphics_api Color
        {
        public:
            buint8 red;
            buint8 green;
            buint8 blue;
            buint8 alpha;

            Color();
            Color(buint8 red, buint8 green, buint8 blue, buint8 alpha);
            
            // Static data members НЕ наследуют атрибут класса (MSVC):
            // отдельный макрос данных — __blib_data_api (dllimport у
            // потребителей, пусто в сборке blib-graphics)
            static const __blib_data_api Color Black;
            static const __blib_data_api Color BlackAlpha;
            static const __blib_data_api Color White;
            static const __blib_data_api Color Red;
            static const __blib_data_api Color Transparent;

            static buint8 bytesPerPixel() { return 4; }
        };

        typedef std::vector<Color> Colors;
        std::vector<float>makeFloatData(const Colors& colors);

    }
}
