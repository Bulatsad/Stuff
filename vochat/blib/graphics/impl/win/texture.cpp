#include <blib/graphics/texture.h>

#include <Windows.h>
#include <gl/GL.h>

#define __blib_this_context(_this) (*(static_cast<GLuint*>(this->ctx)))
#define __blib_get_gl_texture_id(_this) __blib_this_context(_this)

blib::graphics::Texture::Texture()
{
    this->ctx = new GLuint;
}

blib::graphics::Texture::~Texture()
{
    glDeleteTextures(1, &__blib_get_gl_texture_id(this));
}

void blib::graphics::Texture::create(const blib::graphics::Image& image)
{
    this->width  = image.width;
    this->height = image.height;

    glEnable(GL_TEXTURE_2D);
    glGenTextures(1, &__blib_get_gl_texture_id(this));
    glBindTexture(GL_TEXTURE_2D, __blib_get_gl_texture_id(this));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, this->width, this->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.getData());
}

void* blib::graphics::Texture::getContext() const
{
    return this->ctx;
}
