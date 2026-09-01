#pragma once

#include <blib/config.h>

#include <blib/blibint.h>

#include <blib/graphics/image.h>
#include <blib/graphics/rendercontext.h>

#ifdef __blib_render_api_opengl
// texture platform depended (PD) context 
static struct win_gl_TexturePDCtx
{
    GLuint textureID = 0;
};
typedef win_gl_TexturePDCtx TexturePDCtx;
#endif // __blib_render_api_opengl

// blib abstracted texture context type
typedef TexturePDCtx TextureCtx;

namespace blib
{
    namespace graphics
    {
        class __blib_graphics_api RenderContext;
        class __blib_graphics_api Texture
        {
        private:
            TexturePDCtx ctx;

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

            TextureCtx getContext() const;

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
