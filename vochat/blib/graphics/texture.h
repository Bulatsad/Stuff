#pragma once

#include <blib/config.h>

#include <blib/blibint.h>

#include <blib/graphics/image.h>

namespace blib
{
    namespace graphics
    {
        class __blib_api Texture
        {
        private:
            void* ctx;
        
        public:
            bint16 width;
            bint16 height;

            Texture();
            ~Texture();
            void create(const Image& image);
            void* getContext() const;
        };
    }
}
