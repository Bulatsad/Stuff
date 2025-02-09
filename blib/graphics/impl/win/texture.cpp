#include <blib/graphics/texture.h>

#include <Windows.h>
#include <gl/GL.h>

#define __blib_this_context(_this) (*(static_cast<GLuint*>(this->ctx)))
#define __blib_get_gl_texture_id(_this) __blib_this_context(_this)

#define GL_CLAMP_TO_EDGE 0x812F

blib::graphics::Texture::Texture()
{
    this->ctx = new GLuint;
}

blib::graphics::Texture::~Texture()
{
    glDeleteTextures(1, &__blib_get_gl_texture_id(this));
}

void blib::graphics::Texture::create(const blib::graphics::Image& image, genFlags flags)
{
    this->width  = image.width;
    this->height = image.height;

    glEnable(GL_TEXTURE_2D);
    glGenTextures(1, &__blib_get_gl_texture_id(this));
    glBindTexture(GL_TEXTURE_2D, __blib_get_gl_texture_id(this));

    // TODO : rewrite normaly
    if (static_cast<buint8>(flags) & static_cast<buint8>(genFlags::clamp_to_edge))
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, this->width, this->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.getData());
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
