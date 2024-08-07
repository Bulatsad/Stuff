#pragma once

#include <blib/config.h>

#include <blib/blibint.h>
#include <blib/graphics/color.h>

#include <vector>

namespace blib
{
    namespace graphics
    {
        class Image
        {
        private:
            std::vector<Color> bitmap;
        
        public:
            buint16 width;
            buint16 height;

            __blib_api Image();
            __blib_api ~Image();

            __blib_api bool loadFromTgx(const char* path);
            __blib_api const void* getData() const;
        };
    }
}
