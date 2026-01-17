#include <blib/graphics/texture.h>

#include <iostream>
#include <stdexcept>

#include <Windows.h>
#include <gl/GL.h>

#define __blib_this_context(_this) (this->ctx.textureID)
#define __blib_get_gl_texture_id(_this) __blib_this_context(_this)

#define GL_CLAMP_TO_EDGE 0x812F

blib::graphics::Texture::Texture()
{
}

blib::graphics::Texture::~Texture()
{
    if (__blib_get_gl_texture_id(this))
    {
        std::cerr << "Memory leak detected" << std::endl;
    }
}

void blib::graphics::Texture::create(const blib::graphics::Image& image, blib::graphics::RenderContext& ctx, genFlags flags)
{
    this->create(reinterpret_cast<const void*>(image.getData()), image.width, image.height, 4, ctx, flags);
}

int blib::graphics::Texture::create(const void* pdata, bint16 width, bint16 height, buint8 bytesPerPixel, blib::graphics::RenderContext& ctx, genFlags flags)
{
    this->width = width;
    this->height = height;

    // create texure id
    ctx.api.ogl.ext.__blib_gl_glGenTextures(1, &__blib_get_gl_texture_id(this));

    // bind texture
    ctx.api.ogl.ext.__blib_gl_glBindTexture(GL_TEXTURE_2D, __blib_get_gl_texture_id(this));

    // populate texture
    switch (bytesPerPixel)
    {
    case 3:
        ctx.api.ogl.ext.__blib_gl_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, this->width, this->height, 0, GL_RGB, GL_UNSIGNED_BYTE, pdata);
        break;
    case 4:
        ctx.api.ogl.ext.__blib_gl_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, this->width, this->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pdata);
        break;
    default:
        throw new std::runtime_error("not implemented");
        break;
    }

    // TODO : rewrite normaly
    if (static_cast<buint8>(flags) & static_cast<buint8>(genFlags::clamp_to_edge))
    {
        ctx.api.ogl.ext.__blib_gl_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        ctx.api.ogl.ext.__blib_gl_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    ctx.api.ogl.ext.__blib_gl_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    ctx.api.ogl.ext.__blib_gl_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);


    // Unbind texture
    ctx.api.ogl.ext.__blib_gl_glBindTexture(GL_TEXTURE_2D, 0);

    return 0;
}

void blib::graphics::Texture::free(blib::graphics::RenderContext& ctx)
{
    ctx.api.ogl.ext.__blib_gl_glDeleteTextures(1, &__blib_get_gl_texture_id(this));
    __blib_get_gl_texture_id(this) = 0;
}

TextureCtx blib::graphics::Texture::getContext() const
{
    return this->ctx;
}

void blib::graphics::Texture::update(const std::vector<blib::graphics::Color>&, unsigned int width, unsigned int height, unsigned int x, unsigned int y, blib::graphics::RenderContext& ctx)
{
    (ctx.api.ogl.ext.__blib_gl_glBindTexture(GL_TEXTURE_2D,  __blib_get_gl_texture_id(this)));
    //(glTexSubImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(x), static_cast<GLint>(y), static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_RGBA, GL_UNSIGNED_BYTE, pixels));
    (ctx.api.ogl.ext.__blib_gl_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    
    ctx.api.ogl.ext.__blib_gl_glFlush();
}

blib::graphics::Texture blib::graphics::Texture::makeTextureAtlas(const std::vector<Texture>& textures)
{
    buint32 resWidth = 0;
    buint32 resHeight = 0;

    for (const blib::graphics::Texture& t : textures)
    {
        if (resWidth + t.width >= buint16Max)
        {
            throw std::runtime_error("texture atlas is to large");
        }
        if (resHeight + t.height >= buint16Max)
        {
            throw std::runtime_error("texture atlas is to large");
        }

        resWidth += t.width;
        if (resHeight < t.height)
            resHeight = t.height;
    }

    blib::graphics::Image img(resHeight,resHeight);
    //img.update();

}
