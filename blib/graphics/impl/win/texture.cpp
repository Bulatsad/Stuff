#include <blib/graphics/texture.h>

#include <Windows.h>
#include <gl/GL.h>

#define __blib_this_context(_this) (*(static_cast<GLuint*>(this->ctx)))
#define __blib_get_gl_texture_id(_this) __blib_this_context(_this)

#define GL_CLAMP_TO_EDGE 0x812F

blib::graphics::Texture::Texture()
{
    this->ctx = new GLuint;
    memset(this->ctx, 0, sizeof(GLuint));
}

blib::graphics::Texture::~Texture()
{
    glDeleteTextures(1, &__blib_get_gl_texture_id(this));
}

void blib::graphics::Texture::create(const blib::graphics::Image& image, genFlags flags)
{
    this->create(reinterpret_cast<const void*>(image.getData()), image.width, image.height, 4, flags);
}

int blib::graphics::Texture::create(const void* pdata, bint16 width, bint16 height, buint8 bytesPerPixel, genFlags flags)
{
    this->width = width;
    this->height = height;

    glGenTextures(1, &__blib_get_gl_texture_id(this));

    glBindTexture(GL_TEXTURE_2D, __blib_get_gl_texture_id(this));

    switch (bytesPerPixel)
    {
    case 3:
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, this->width, this->height, 0, GL_RGB, GL_UNSIGNED_BYTE, pdata);
        break;
    case 4:
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, this->width, this->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pdata);
        break;
    default:
        throw new std::exception("not implemented");
        break;
    }


    // TODO : rewrite normaly
    if (static_cast<buint8>(flags) & static_cast<buint8>(genFlags::clamp_to_edge))
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);



    return 0;
}

void* blib::graphics::Texture::getContext() const
{
    return this->ctx;
}

void blib::graphics::Texture::update(const std::vector<blib::graphics::Color>, unsigned int width, unsigned int height, unsigned int x, unsigned int y)
{
    (glBindTexture(GL_TEXTURE_2D,  __blib_get_gl_texture_id(this)));
    //(glTexSubImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(x), static_cast<GLint>(y), static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_RGBA, GL_UNSIGNED_BYTE, pixels));
    (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    
    glFlush();
}
