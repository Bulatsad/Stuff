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

            enum class genFlags : buint8
            {
                none,
                clamp_to_edge
            };

            bint16 width;
            bint16 height;

            Texture();
            ~Texture();
            void create(const Image& image, genFlags flags = genFlags::none);
            void* getContext() const;

            void update(
                const std::vector<blib::graphics::Color>,
                unsigned int width,
                unsigned int height,
                unsigned int x,
                unsigned int y
            );
        };
    }
}
