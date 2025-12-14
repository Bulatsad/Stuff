#pragma once

#include <blib/config.h>

#include <blib/blibint.h>

#include <blib/graphics/image.h>
#include <blib/graphics/rendercontext.h>

namespace blib
{
    namespace graphics
    {
        class __blib_api RenderContext;
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
            void create(const Image& image, blib::graphics::RenderContext& ctx, genFlags flags = genFlags::none);
            int create(const void* pdata, bint16 width, bint16 height, buint8 bytesPerPixel, blib::graphics::RenderContext& ctx, genFlags flags = genFlags::none);
            void free(blib::graphics::RenderContext& ctx);

            void* getContext() const;

            void update(
                const std::vector<blib::graphics::Color>&,
                unsigned int width,
                unsigned int height,
                unsigned int x,
                unsigned int y,
                blib::graphics::RenderContext& ctx
            );

            static Texture makeTextureAtlas(const std::vector<Texture>& textures);
        };
    }
}
